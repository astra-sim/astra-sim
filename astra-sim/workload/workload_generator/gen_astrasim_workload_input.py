from absl import app
from absl import flags
import os
import subprocess
import configparser as cp

# Common variables for all layers
DatatypeSize            = 2
NumberOfNPUs            = 8 # This represents the total number of NPUs in the whole system
NumberOfPackages        = 1
NumberOfNPUsPerPackage  = 0
Strides                 = 1
FilterHeight            = 1
NumberOfChannels        = 1
# End common variables for all layers

# Region for constants
SCALESIM_PATH = r"../../compute/SCALE-Sim"
SCALESIM_CONFIG = r"../../compute/SCALE-Sim/configs/google.cfg"
OUTPUT_FILE_NAME = r"workload.txt"

Hybrid                  = "HYBRID"
ForwardPassCycles       = "fwd_pass_cycles"
InputGradientCycles     = "inp_grad_cycles"
WeightGradientCycles    = "weight_grad_cycles"
DataParallel            = "DATA"
ModelParallel           = "MODEL"
HybridDataModelParallel = "HYBRID_DATA_MODEL"
HybridModelDataParallel = "HYBRID_MODEL_DATA"
AllToAll                = "ALLTOALL"
AllReduce               = "ALLREDUCE"
AllGather               = "ALLGATHER"
NoCommunication         = "NONE"
Outputs                 = "outputs"
ParallelizationStrategy = ModelParallel
# End region for constants

# Region for command line arguments

FLAGS = flags.FLAGS
#name of flag | default | explanation
flags.DEFINE_string("mnk", "mnk_inputs/test.csv", "Path of the file that has the m,n,k values")
flags.DEFINE_string("run_name", "test", "Name of the folder that will have the generated output")
flags.DEFINE_string("output_file", OUTPUT_FILE_NAME, "Name of the generated ASTRA-Sim input file")
flags.DEFINE_string("scalesim_path", SCALESIM_PATH, "Path to SCALE-Sim folder")
flags.DEFINE_string("scalesim_config", SCALESIM_CONFIG, "Path to SCALE-Sim config file")
flags.DEFINE_string("parallel", DataParallel, "Parallelization strategy: MODEL, DATA, HYBRID_DATA_MODEL, HYBRID_MODEL_DATA")
flags.DEFINE_string("datatype_size", str(DatatypeSize), "Size of the data type")
flags.DEFINE_string("num_npus", str(NumberOfNPUs), "Total number of NPUs")
flags.DEFINE_string("num_packages", str(NumberOfPackages), "Number of packages")

def parseCommandLineArguments():
    global ParallelizationStrategy, DatatypeSize, NumberOfNPUs, NumberOfPackages, NumberOfNPUsPerPackage
    global SCALESIM_PATH, SCALESIM_CONFIG, OUTPUT_FILE_NAME
    SCALESIM_PATH = os.path.abspath(FLAGS.scalesim_path)
    SCALESIM_CONFIG = os.path.abspath(FLAGS.scalesim_config)
    if not os.path.exists(SCALESIM_PATH):
        print('ERROR: SCALE-Sim not exists in {}'.format(SCALESIM_PATH))
        print('  run "git clone https://github.com/ARM-software/SCALE-Sim.git" to clone SCALE-Sim to the proper place')
        print('  or run build.sh in astra-sim root directory and find SCALE-Sim under astra-sim/compute/')
        exit()
    if not os.path.exists(SCALESIM_CONFIG):
        print('ERROR: SCALE-Sim config "{}" not found'.format(SCALESIM_CONFIG))
        exit()
    OUTPUT_FILE_NAME = os.path.abspath(FLAGS.output_file)
    ParallelizationStrategy = FLAGS.parallel
    DatatypeSize = int(FLAGS.datatype_size)
    NumberOfNPUs = int(FLAGS.num_npus)
    NumberOfPackages = int(FLAGS.num_packages)
    NumberOfNPUsPerPackage = int(NumberOfNPUs / NumberOfPackages)
    assert NumberOfNPUsPerPackage * NumberOfPackages == NumberOfNPUs

DEBUG = 1

def DPRINT(message):
    if DEBUG == 1:
        print (message)

class ModelParallelStrategy:

    def __init__(self):
        pass

    def getCommunicationTypeForFwdPass(self, i, layer):
        return AllGather

    def getCommunicationTypeForInpGrad(self, i, layer):
        return AllReduce

    def getCommunicationTypeForWeightGrad(self, i, layer):
        return NoCommunication

    def getCommunicationSizeForFwdPass(self, i, layer):
        return int(layer.m * layer.n * DatatypeSize / NumberOfNPUs)

    def getCommunicationSizeForInpGrad(self, i, layer):
        return int(layer.m * layer.k * DatatypeSize)

    def getCommunicationSizeForWeightGrad(self, i, layer):
        return 0

class DataParallelStrategy:

    def __init__(self):
        pass

    def getCommunicationTypeForFwdPass(self, i, layer):
        return NoCommunication

    def getCommunicationTypeForInpGrad(self, i, layer):
        return NoCommunication

    def getCommunicationTypeForWeightGrad(self, i, layer):
        return AllReduce

    def getCommunicationSizeForFwdPass(self, i, layer):
        return 0

    def getCommunicationSizeForInpGrad(self, i, layer):
        return 0

    def getCommunicationSizeForWeightGrad(self, i, layer):
        return int(layer.n * layer.k * DatatypeSize)

# HybridDataModel: data-parallel between packages, model-parallel within package
class HybridDataModelParallelStrategy:

    def __init__(self):
        pass

    def getCommunicationTypeForFwdPass(self, i, layer):
        return AllGather

    def getCommunicationTypeForInpGrad(self, i, layer):
        return AllReduce

    def getCommunicationTypeForWeightGrad(self, i, layer):
        return AllReduce

    def getCommunicationSizeForFwdPass(self, i, layer): # within package
        return int(layer.m * layer.n * DatatypeSize / NumberOfNPUs)

    def getCommunicationSizeForInpGrad(self, i, layer): # within package
        return int(layer.m * layer.k * DatatypeSize / NumberOfPackages)

    def getCommunicationSizeForWeightGrad(self, i, layer): # between package
        return int(layer.k * layer.n * DatatypeSize / NumberOfNPUsPerPackage)

# HybridModelData: model-parallel between pacakges, data-parallel within pacakge
class HybridModelDataParallelStrategy:

    def __init__(self):
        pass

    def getCommunicationTypeForFwdPass(self, i, layer):
        return AllGather

    def getCommunicationTypeForInpGrad(self, i, layer):
        return AllReduce

    def getCommunicationTypeForWeightGrad(self, i, layer):
        return AllReduce

    def getCommunicationSizeForFwdPass(self, i, layer): # between packages, all-gather
        return int(layer.m * layer.n * DatatypeSize / NumberOfNPUs)

    def getCommunicationSizeForInpGrad(self, i, layer): # between packages, all-reduce
        return int(layer.m * layer.k * DatatypeSize / NumberOfNPUsPerPackage)

    def getCommunicationSizeForWeightGrad(self, i, layer): # within packages
        return int(layer.n * layer.k * DatatypeSize / NumberOfPackages)

class AstraSimOutput:
    def __init__(self, layers, scaleSimOutput):
        self.layers = layers
        self.scaleSimOutput = scaleSimOutput
        self.strategy = {}
        self.strategy[ModelParallel] = ModelParallelStrategy()
        self.strategy[DataParallel] = DataParallelStrategy()
        self.strategy[HybridDataModelParallel] = HybridDataModelParallelStrategy()
        self.strategy[HybridModelDataParallel] = HybridModelDataParallelStrategy()

        self.activeStrategy = self.strategy[ParallelizationStrategy]

        self.output = []

    def generate(self):
        self.output = []
        length = len(self.layers)
        for i in range(0, length):
            line = []
            line.append(self.layers[i].name) # Layer name
            line.append("-1") # Reserved variable
            line.append(self.scaleSimOutput[ForwardPassCycles][i]) # Forward pass compute time
            line.append(self.activeStrategy.getCommunicationTypeForFwdPass(i, self.layers[i])) # Forward pass communication type
            line.append(self.activeStrategy.getCommunicationSizeForFwdPass(i, self.layers[i])) # Forward pass communication size
            line.append(self.scaleSimOutput[InputGradientCycles][i]) # Input gradient compute time
            line.append(self.activeStrategy.getCommunicationTypeForInpGrad(i, self.layers[i])) # Input gradient communication type
            line.append(self.activeStrategy.getCommunicationSizeForInpGrad(i, self.layers[i])) # Input gradient communication size
            line.append(self.scaleSimOutput[WeightGradientCycles][i]) # Weight gradient compute time
            line.append(self.activeStrategy.getCommunicationTypeForWeightGrad(i, self.layers[i])) # Weight gradient communication type
            line.append(self.activeStrategy.getCommunicationSizeForWeightGrad(i, self.layers[i])) # Weight gradient communication size
            line.append(100) # Delay for 1KB communication size

            line = map(lambda x: str(x), line)
            line_string = "\t".join(line)

            DPRINT(line_string)
            self.output.append(line_string)

    def writeToFile(self):
        # TODO: Right now, the first few in the file are hardcoded. Instead, we must find a way to generate this
        file_handle = open(OUTPUT_FILE_NAME, "w")
        file_handle.write(ParallelizationStrategy + "\n")
        file_handle.write(str(len(self.output)))
        for line in self.output:
            file_handle.write("\n")
            file_handle.write(line)
        file_handle.close()


class TopologyItem:
    def __init__(self, name, ifmap_height, ifmap_width, filter_height, filter_width, channels, num_filters, strides):
        self.name = name
        self.ifmap_height = ifmap_height
        self.ifmap_width = ifmap_width
        self.filter_height = filter_height
        self.filter_width = filter_width
        self.channels = channels
        self.num_filters = num_filters
        self.strides = strides

    def print(self):
        line = [self.name, self.ifmap_height, self.ifmap_width, self.filter_height, self.filter_width, self.channels, self.num_filters, self.strides]
        line = list(map(lambda x: str(x), line))
        line = "\t".join(line)
        DPRINT(line)

    def write(self, file_handle):
        line = [self.name, self.ifmap_height, self.ifmap_width, self.filter_height, self.filter_width, self.channels, self.num_filters, self.strides, ""]
        line = list(map(lambda x: str(x), line))
        file_handle.write(",".join(line))
        file_handle.write("\n")

    @staticmethod
    def printHeader():
         DPRINT("Layer name\tifmap height\tifmap width\tfilter height\tfilter width\tchannels\tnum filter\tstrides")

    @staticmethod
    def writeHeaderToFile(file_handle):
        file_handle.write("Layer name,ifmap height,ifmap width,filter height,filter width,channels,num filter,strides,\n")

class Layer:
    def __init__(self, name, m, n, k):
        self.name = name
        self.m = int(m) # batch size
        self.n = int(n) # output dimension
        self.k = int(k) # input dimension
        self.parallelism = ParallelizationStrategy

    def __init__(self, row):
        self.name = row[0]
        self.m = int(row[1])
        self.n = int(row[2])
        self.k = int(row[3])
        self.parallelism = ParallelizationStrategy

    def print(self):
        print (self.name + ", " + self.m + ", " + self.n + ", " + self.k + ", " + self.parallelism)

def writeGeneratedTopologyToFile(folder_name, file_name, items):
    current_path = os.getcwd()

    # Create the output directory if does not exist and change to output directory
    output_folder_path = os.path.join(current_path, Outputs)
    if not os.path.exists(output_folder_path):
        os.makedirs(Outputs)
    os.chdir(output_folder_path)

    output_folder_path = os.path.join(output_folder_path, folder_name)
    if not os.path.exists(output_folder_path):
        os.makedirs(folder_name)
    os.chdir(output_folder_path)

    # Create the scale sim output directory if does not exist
    #run_name_path = os.path.join(output_folder_path, "scaleSimOutput")
    #if not os.path.exists(run_name_path):
    #    os.makedirs("scaleSimOutput")

    # Create the run name directory if does not exist and change change to run name directory
    run_name_path = os.path.join(output_folder_path, FLAGS.run_name)
    if not os.path.exists(run_name_path):
        os.makedirs(FLAGS.run_name)
    os.chdir(run_name_path)

    # Write the generated topology to a file
    file_handle = open(file_name, "w")
    TopologyItem.writeHeaderToFile(file_handle)
    for row in items:
        row.write(file_handle)
    file_handle.close()

    # Go back to old directory
    os.chdir(current_path)

def runScaleSim(topology_file, parallelism):
    current_path = os.getcwd()
    full_path = os.path.join(os.getcwd(), Outputs, parallelism, FLAGS.run_name, topology_file)
    os.chdir(SCALESIM_PATH)
    process = subprocess.Popen(["python3", "scale.py", "-arch_config=" + SCALESIM_CONFIG, "-network="+full_path])
    process.wait()
    os.chdir(current_path)

def getCylesFromScaleSimOutput(folder_name, topology_file):
    config = cp.ConfigParser()
    config.read(SCALESIM_CONFIG)
    run_name = config.get('general', 'run_name').strip('"')

    cycles_filename = topology_file.rstrip("csv").rstrip(".") + "_cycles.csv" # remove .csv frim topology file and makes it _cycles.csv

    current_path = os.getcwd()
    full_path = os.path.join(SCALESIM_PATH, "outputs", run_name)
    os.chdir(full_path)
    cpy_path = os.path.join(current_path, Outputs, folder_name, FLAGS.run_name)
    process = subprocess.call("cp -rf " + cycles_filename + " " + cpy_path, shell=True)

    file_handle = open(cycles_filename, "r")
    lines = file_handle.readlines()
    lines = map(lambda x: x.strip("\n"), lines) # removes the newline characters from the end

    first = True
    cycles = []

    for line in lines:
        if first:
            first = False
            continue
        cycles.append(line.split(',')[1].strip('\t')) # '\t removes the last preceeding tab character preesnt in scalesim output

    os.chdir(current_path)
    return cycles

def getScaleSimOutputInternal(fwd_pass, inp_grad, weight_grad, parallel_mode):
    fwd_pass_filename = os.path.basename(FLAGS.mnk).rstrip(".csv") + "_fwd_pass.csv"
    inp_grad_filename = os.path.basename(FLAGS.mnk).rstrip(".csv") + "_inp_grad.csv"
    weight_grad_filename = os.path.basename(FLAGS.mnk).rstrip(".csv") + "_weight_grad.csv"
    writeGeneratedTopologyToFile(parallel_mode, fwd_pass_filename, fwd_pass)
    writeGeneratedTopologyToFile(parallel_mode, inp_grad_filename, inp_grad)
    writeGeneratedTopologyToFile(parallel_mode, weight_grad_filename, weight_grad)

    runScaleSim(fwd_pass_filename, parallel_mode)
    runScaleSim(inp_grad_filename, parallel_mode)
    runScaleSim(weight_grad_filename, parallel_mode)

    fwd_pass_cycles = getCylesFromScaleSimOutput(parallel_mode, fwd_pass_filename)
    inp_grad_cycles = getCylesFromScaleSimOutput(parallel_mode, inp_grad_filename)
    weight_grad_cycles = getCylesFromScaleSimOutput(parallel_mode, weight_grad_filename)

    return { ForwardPassCycles : fwd_pass_cycles, InputGradientCycles : inp_grad_cycles, WeightGradientCycles : weight_grad_cycles }

def getScaleSimOutputForDataParallelApproach(layers):
    fwd_pass = []
    for layer in layers:
        fwd_pass.append(TopologyItem(layer.name, int(layer.m / NumberOfNPUs), layer.k, FilterHeight, layer.k, NumberOfChannels, layer.n, Strides))

    inp_grad = []
    for layer in layers:
        inp_grad.append(TopologyItem(layer.name, int(layer.m / NumberOfNPUs), layer.n, FilterHeight, layer.n, NumberOfChannels, layer.k, Strides))

    weight_grad = []
    for layer in layers:
        weight_grad.append(TopologyItem(layer.name, layer.k, int(layer.m / NumberOfNPUs), FilterHeight, int(layer.m / NumberOfNPUs), NumberOfChannels, layer.n, Strides))

    return getScaleSimOutputInternal(fwd_pass, inp_grad, weight_grad, DataParallel)

def getScaleSimOutputForModelParallelApproach(layers):
    fwd_pass = []
    for layer in layers:
        fwd_pass.append(TopologyItem(layer.name, layer.m, layer.k, FilterHeight, layer.k, NumberOfChannels, int(layer.n / NumberOfNPUs), Strides))

    inp_grad = []
    for layer in layers:
        inp_grad.append(TopologyItem(layer.name, layer.m, int(layer.n / NumberOfNPUs), FilterHeight, int(layer.n / NumberOfNPUs), NumberOfChannels, layer.k, Strides))

    weight_grad = []
    for layer in layers:
        weight_grad.append(TopologyItem(layer.name, layer.k, layer.m, FilterHeight, layer.m, NumberOfChannels, int(layer.n / NumberOfNPUs), Strides))

    return getScaleSimOutputInternal(fwd_pass, inp_grad, weight_grad, ModelParallel)

# HybridDataModel: data-parallel between packages, model-parallel within package
def getScaleSimOutputForHybridDataModelParallelApproach(layers):
    fwd_pass = []
    for layer in layers:
        fwd_pass.append(TopologyItem(layer.name, int(layer.m / NumberOfPackages), layer.k, FilterHeight, layer.k, NumberOfChannels, int(layer.n / NumberOfNPUsPerPackage), Strides))

    inp_grad = []
    for layer in layers:
        inp_grad.append(TopologyItem(layer.name, int(layer.m / NumberOfPackages), int(layer.n / (NumberOfNPUsPerPackage)), FilterHeight, int(layer.n / NumberOfNPUsPerPackage), NumberOfChannels, layer.k, Strides))

    weight_grad = []
    for layer in layers:
        weight_grad.append(TopologyItem(layer.name, layer.k, int(layer.m / NumberOfPackages), FilterHeight, int(layer.m / NumberOfPackages), NumberOfChannels, int(layer.n / NumberOfNPUsPerPackage), Strides))

    return getScaleSimOutputInternal(fwd_pass, inp_grad, weight_grad, HybridDataModelParallel)

# HybridModelData: model-parallel between packages, data-parallel within package
def getScaleSimOutputForHybridModelDataParallelApproach(layers):
    fwd_pass = []
    for layer in layers:
        fwd_pass.append(TopologyItem(layer.name, int(layer.m / NumberOfNPUsPerPackage), layer.k, FilterHeight, layer.k, NumberOfChannels, int(layer.n / NumberOfPackages), Strides))

    inp_grad = []
    for layer in layers:
        inp_grad.append(TopologyItem(layer.name, int(layer.m / NumberOfNPUsPerPackage), int(layer.n / NumberOfPackages), FilterHeight, int(layer.n / NumberOfPackages), NumberOfChannels, layer.k, Strides))

    weight_grad = []
    for layer in layers:
        weight_grad.append(TopologyItem(layer.name, layer.k, int(layer.m / NumberOfNPUsPerPackage), FilterHeight, int(layer.m / NumberOfNPUsPerPackage), NumberOfChannels, int(layer.n / NumberOfPackages), Strides))

    return getScaleSimOutputInternal(fwd_pass, inp_grad, weight_grad, HybridModelDataParallel)

def main(argv):
    parseCommandLineArguments()
    mnk_file = FLAGS.mnk
    file_handle = open(mnk_file, "r")
    lines = file_handle.readlines()
    file_handle.close()
    first = True
    layers = []

    for line in lines:
        if first:
            first = False
            continue
        line = line.strip('\n').strip(' ')
        cols = line.split(",")
        assert len(cols) == 4, "There should be 4 columns in the mnk file"
        layers.append(Layer(cols))

    scaleSimOutput = None

    if ParallelizationStrategy == DataParallel:
        scaleSimOutput = getScaleSimOutputForDataParallelApproach(layers)
    elif ParallelizationStrategy == ModelParallel:
        scaleSimOutput = getScaleSimOutputForModelParallelApproach(layers)
    elif ParallelizationStrategy == HybridDataModelParallel:
        scaleSimOutput = getScaleSimOutputForHybridDataModelParallelApproach(layers)
    elif ParallelizationStrategy == HybridModelDataParallel:
        scaleSimOutput = getScaleSimOutputForHybridModelDataParallelApproach(layers)
    else:
        print ("Invalid parallelization strategy")

    astraSimOutput = AstraSimOutput(layers, scaleSimOutput)
    astraSimOutput.generate()
    astraSimOutput.writeToFile()

if __name__ == '__main__':
    app.run(main)
