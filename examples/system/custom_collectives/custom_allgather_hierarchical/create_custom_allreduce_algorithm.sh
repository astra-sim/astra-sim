set -e

echo "Enter the path to where the collectiveapi repo (https://github.com/astra-sim/collectiveapi) is cloned."
read -p "Press Enter to use default path (\$HOME/collectiveapi), or type 'abort' or 'x' to exit: " COLLECTIVEAPI_PATH

if [ -z "$COLLECTIVEAPI_PATH" ]; then
    COLLECTIVEAPI_PATH="$HOME/collectiveapi"
fi
if [ "$COLLECTIVEAPI_PATH" = "abort" ] || [ "$COLLECTIVEAPI_PATH" = "x" ]; then
    echo "Aborted by user."
    exit 1
fi
echo "COLLECTIVEAPI_PATH: $COLLECTIVEAPI_PATH"

SCRIPT_DIR=$(dirname "$(realpath "$0")")

# echo "Generating XML representation of custom allreduce algorithm from MSCCLang..."
# python $COLLECTIVEAPI_PATH/msccl-tools/examples/mscclang/allreduce_a100_ring.py 8 1 1 > $SCRIPT_DIR/allgather_hierarchical.xml

echo "Converting XML to Chakra ET representation..."
python $COLLECTIVEAPI_PATH/chakra_converter/et_converter.py --input_filename $SCRIPT_DIR/allgather_hierarchical.xml --output_filename $SCRIPT_DIR/allgather_hierarchical 

read -p "Optional: Do you want to convert the Chakra ET representation to JSON? [y/n] " CONVERT_TO_JSON

if [ "$CONVERT_TO_JSON" = "y" ] || [ -z "$CONVERT_TO_JSON" ]; then
    echo "Converting Chakra ET representation to JSON..."
    for i in {0..7}; do
        chakra_jsonizer \
            --input_filename $SCRIPT_DIR/allgather_hierarchical.$i.et \
            --output_filename $SCRIPT_DIR/allgather_hierarchical.$i.json
    done
fi