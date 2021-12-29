"""  查看输入输出结点 """
# import os
# import tensorflow as tf
#
#
# def create_graph(model_dir, model_name):
#     with tf.gfile.FastGFile(os.path.join(model_dir, model_name), 'rb') as f:
#         graph_def = tf.GraphDef()
#         graph_def.ParseFromString(f.read())
#         tf.import_graph_def(graph_def, name='')
#
#
# pdModel_path = 'DataSet/Weights/'
# pdModel_name = 'TrainedWeights_Tiger.pb'
# create_graph(pdModel_path, pdModel_name)
#
# tensor_name_list = [tensor.name for tensor in tf.get_default_graph().as_graph_def().node]
# for tensor_name in tensor_name_list:
#     print(tensor_name, '\n')

"""  convert """
import tensorflow as tf

pbInput_path = r'DataSet/Weights/TrainedWeights_Tiger.pb'
tfliteOutput_path = r'DataSet/Weights/TrainedWeights_Tiger.tflite'

input_tensor_name = ['input_1']
output_tensor_name = ['output_1', 'output_2']
input_tensor_shape = {'input_1': [1, 320, 320, 3]}
converter = tf.lite.TFLiteConverter.from_frozen_graph(
    pbInput_path,
    input_arrays=input_tensor_name,
    output_arrays=output_tensor_name,
    input_shapes=input_tensor_shape)
tfliteModel = converter.convert()

open(tfliteOutput_path, "wb").write(tfliteModel)
