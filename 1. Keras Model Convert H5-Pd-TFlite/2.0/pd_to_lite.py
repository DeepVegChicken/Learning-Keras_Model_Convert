"""  h5 to tflite - tensorflow 2.2.0 """
import os
import cv2
import numpy as np
import tensorflow as tf
from tensorflow.keras.models import *
from tensorflow.keras.layers import *


"""
newtwork
"""


# Data iterator
class Generator(object):
    def __init__(self):
        self.input_size = [128, 128]

        self.image_dir = 'DataSet/test/'
        self.imgSet = [os.path.join(self.image_dir, f) for f in os.listdir(
            self.image_dir) if os.path.isfile(os.path.join(self.image_dir, f))]

    def generate(self):
        for img_path in self.imgSet:
            # Assuming you don't need (/255.0 or other) when testing, skip these steps
            orig_image = cv2.imread(img_path)
            rgb_image = cv2.cvtColor(orig_image, cv2.COLOR_BGR2RGB)
            image_tensor = cv2.resize(rgb_image, dsize=tuple(self.input_size))
            image_tensor = np.asarray(image_tensor / 255.0, dtype=np.float32)
            image_tensor = image_tensor[np.newaxis, :]
            yield [image_tensor]


h5Input_path = 'DataSet/Weights/TrainedWeights_Tiger.h5'
tfliteOutput_path = 'DataSet/Weights/TrainedWeights_Tiger.tflite'

h5Model = YOLONet(Input(shape=(128, 128, 3)), 3, 1)
h5Model.load_weights(h5Input_path)
converter = tf.lite.TFLiteConverter.from_keras_model(h5Model)

# quantitative...
converter.optimizations = [tf.lite.Optimize.OPTIMIZE_FOR_SIZE]
converter.representative_dataset = Generator().generate
converter.target_spec.supported_types = [tf.int8]

# convert
tfliteModel = converter.convert()
# save converted tflite file
open(tfliteOutput_path, "wb").write(tfliteModel)
print("successfully convert to tflite done")
