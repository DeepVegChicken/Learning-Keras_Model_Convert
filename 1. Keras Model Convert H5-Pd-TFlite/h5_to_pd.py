from Model import *
from Utils import *
import os.path as osp
from keras import backend as K
from tensorflow.python.framework import graph_util, graph_io
from tensorflow.python.tools import import_pb_to_tensorboard


def h5_to_pb(h5_model, output_dir, model_name, out_prefix, log_tensorboard=False):
    if osp.exists(output_dir) == False:
        os.mkdir(output_dir)

    # 修改输出结点名称
    out_nodes = []
    for i in range(len(h5_model.outputs)):
        out_nodes.append(str(out_prefix) + str(i + 1))
        tf.identity(h5_model.output[i], str(out_prefix) + str(i + 1))

    sess = K.get_session()
    init_graph = sess.graph.as_graph_def()
    main_graph = graph_util.convert_variables_to_constants(sess, init_graph, out_nodes)
    graph_io.write_graph(main_graph, output_dir, name=model_name, as_text=False)

    if log_tensorboard:
        import_pb_to_tensorboard.import_to_tensorboard(osp.join(output_dir, model_name), output_dir)


h5Model_path = 'DataSet/Weights/TrainedWeights_Tiger.h5'
pbOutput_path = 'DataSet/Weights/'
pbOutput_name = 'TrainedWeights_Tiger.pb'
# # 原输入与输出结点名称
# output_node_names = ["input_1:0", "conv2d_36/BiasAdd:0", "conv2d_38/BiasAdd:0"]

h5Model = YOLONet(Input(shape=(320, 320, 3)), 3, 1)
h5Model.load_weights(h5Model_path)

# h5_to_pb convert
h5_to_pb(h5Model,
         output_dir=pbOutput_path,
         model_name=pbOutput_name,
         out_prefix='output_')
