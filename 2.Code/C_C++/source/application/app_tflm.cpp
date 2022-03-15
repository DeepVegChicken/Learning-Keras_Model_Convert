/*
 * app_tflm.cpp
 *
 *  Created on: 2022年2月10日
 *      Author: niu
 */
#include <stdio.h>
#include <cr_section_macros.h>

#include "tensorflow/lite/version.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "app_tflm.h"
#include "utils.h"
#include "camera_display.h"

#include <math.h>

float anchor[2][3][2] = {
		{{82.625000,110.592593}, {29.583333,73.333333}, {10.666667,18.222222}},
		{{39.666667,42.666667}, {56.916667,64.888889}, {21.500000,32.444444}},
};

__BSS(RAM2) alignas(8) unsigned char ram_model[RAM_MODEL_SIZE];

constexpr int kTensorArenaSize = 768*1024;
__BSS(RAM5) uint8_t tensor_arena[kTensorArenaSize];

tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output[3] = { nullptr };


void setup(void* pModel) {
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(pModel);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // This pulls in all the operation implementations we need.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);

  /* yolo 有多个输出层 */
  for(uint32_t i = 0; i < interpreter->outputs_size(); i++) {
	  output[i] = interpreter->output(i);
  }
}

void print_model_msg(void)
{
	printf("\r\n---------- model msg ----------\r\n");
	printf("in 0 zero_point %ld scale %f\r\n", 		input->params.zero_point, output[0]->params.scale);
	printf("out 0 zero_point %ld scale %f\r\n", output[0]->params.zero_point, output[0]->params.scale);
	printf("out 1 zero_point %ld scale %f\r\n", output[1]->params.zero_point, output[1]->params.scale);
	printf("[input] ");
	printf(" size %d\r\n", interpreter->inputs_size());
	for(uint32_t i = 0; i < interpreter->inputs_size(); i++) {
		printf("\t input-%ld bytes: %d type: %s \t[", i, interpreter->input(i)->bytes, TfLiteTypeGetName(interpreter->input(i)->type));
		for(int n = 0; n < interpreter->input(i)->dims->size; n++)
			printf("%d ", interpreter->input(i)->dims->data[n]);
		printf("]\r\n");
	}

	printf("[output]");
	printf(" size %d\r\n", interpreter->outputs_size());
	for(uint32_t i = 0; i < interpreter->outputs_size(); i++) {
		printf("\t outputs-%ld bytes: %d type: %s \t[", i, output[i]->bytes, TfLiteTypeGetName(output[i]->type));
		for(int n = 0; n < output[i]->dims->size; n++)
			printf("%d ", output[i]->dims->data[n]);
		printf("]\r\n");
	}

	printf("\r\n-------------------------------\r\n");
}

float sigmoid(float x)
{
	float y = 1/(1+expf(-x));
	return y;
}

void print_output(void)
{
	for(uint32_t i = 0; i < interpreter->outputs_size(); i++) {
		printf("[output %ld]\r\n", i);

		uint32_t len_x = output[i]->dims->data[1];
		uint32_t len_y = output[i]->dims->data[2];
		uint32_t len_z = output[i]->dims->data[3];

		for(uint32_t i_x = 0; i_x < len_x; i_x++) {
			for(uint32_t i_y = 0; i_y < len_y; i_y++) {
				for(uint32_t i_z = 0; i_z < len_z; i_z++) {
					if(i_z % 6 == 4) {
						float anchor_x = anchor[i][i_z/6][0];
						float anchor_y = anchor[i][i_z/6][1];
						uint32_t grid_size = 96 / len_x;
						uint32_t k = (i_x*len_y + i_y)*len_z + i_z;
						float x = ((float)output[i]->data.int8[k] - output[i]->params.zero_point) * output[i]->params.scale;
						if(sigmoid(x) > 0.5) {
							int32_t pos_x = grid_size * (i_y + sigmoid((output[i]->data.int8[k-3] - output[i]->params.zero_point) * output[i]->params.scale));
							int32_t pos_y = grid_size * (i_x + sigmoid((output[i]->data.int8[k-4] - output[i]->params.zero_point) * output[i]->params.scale));
							int32_t pos_w = anchor_x * expf((output[i]->data.int8[k-2] - output[i]->params.zero_point) * output[i]->params.scale);
							int32_t pos_h = anchor_y * expf((output[i]->data.int8[k-1] - output[i]->params.zero_point) * output[i]->params.scale);
							printf("[%ld %ld] [conf %.2f] ", i_x, i_y, sigmoid(x));
							printf("[%ld  %ld  %ld  %ld]\r\n", pos_x, pos_y, pos_w, pos_h);
						}
					}
				}
			}
		}
	}

	printf("\r\n");
}

extern "C" {
int32_t dump_image_to_bmp_file(const char *file_path, uint8_t *image, uint32_t width, uint32_t height);

k_task_t detect_task;
uint8_t detect_task_stk[DETECT_STK_SIZE];

void detect_entry(void *arg)
{
    k_err_t err;
    k_event_flag_t flag_match;

	fatfs_get_model(ram_model, sizeof(ram_model));
	setup(ram_model);
	print_model_msg();

	printf("detect init done\r\n");

	while(1) {
    	err = tos_event_pend(&event, event_detect, &flag_match, TOS_TIME_FOREVER, TOS_OPT_EVENT_PEND_ALL | TOS_OPT_EVENT_PEND_CLR);

    	input_data_process((uint32_t *)lcdFrameAddr);
    	k_tick_t time = tos_systick_get();
		if (err == K_ERR_NONE) {
			TfLiteStatus invoke_status = interpreter->Invoke();
			if (invoke_status != kTfLiteOk) {
				TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed\n");
			} else {
				printf("invoke time %lld    ", tos_systick_get() - time);
			}
			tos_event_post_keep(&event, event_detect_done);
		}

		clear_top_n();
		for(uint32_t i = 0; i < interpreter->outputs_size(); i++) {
			printf("[output %ld]\r\n", i);

			uint32_t len_x = output[i]->dims->data[1];
			uint32_t len_y = output[i]->dims->data[2];
			uint32_t len_z = output[i]->dims->data[3];

			for(uint32_t i_x = 0; i_x < len_x; i_x++) {
				for(uint32_t i_y = 0; i_y < len_y; i_y++) {
					for(uint32_t i_z = 0; i_z < len_z; i_z++) {
						if(i_z % 6 == 4) {
							uint32_t grid_size = input->dims->data[1] / len_x;
							uint32_t k = (i_x*len_y + i_y)*len_z + i_z;
							float conf = sigmoid(((float)output[i]->data.int8[k] - output[i]->params.zero_point) * output[i]->params.scale);
							if(conf > 0.7) {
								float anchor_x = anchor[i][i_z/6][0];
								float anchor_y = anchor[i][i_z/6][1];
								int32_t pos_x = grid_size * (i_y + sigmoid((output[i]->data.int8[k-3] - output[i]->params.zero_point) * output[i]->params.scale));
								int32_t pos_y = grid_size * (i_x + sigmoid((output[i]->data.int8[k-4] - output[i]->params.zero_point) * output[i]->params.scale));
								int32_t pos_w = anchor_x * expf((output[i]->data.int8[k-2] - output[i]->params.zero_point) * output[i]->params.scale);
								int32_t pos_h = anchor_y * expf((output[i]->data.int8[k-1] - output[i]->params.zero_point) * output[i]->params.scale);
								printf("[conf %.2f] [%ld  %ld  %ld  %ld]\r\n", conf, pos_x, pos_y, pos_w, pos_h);
								push_top_n(conf, pos_x - pos_w/2, pos_y - pos_w/2, pos_x + pos_w/2, pos_y + pos_w/2, i);
							}
						}
					}
				}
			}
		}
		if(nms(0.3, (uint32_t *)lcdFrameAddr)){
			dump_image_to_bmp_file("-", (uint8_t *)lcdFrameAddr, 800, 480);
			tos_event_post_keep(&event_2, event_mqtt);
		}
	}
}
} // extern "C"


