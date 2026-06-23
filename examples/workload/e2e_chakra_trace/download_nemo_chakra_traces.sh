set -eux

echo 'Running dataset download script'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

gdown 1lz6VCqQ-n5lSyshH0XKSqdynKOVRqGZs -O ${SCRIPT_DIR}/nemo-chakra-mixtral-8x7B-traces.zip
tar -xzvf ${SCRIPT_DIR}/nemo-chakra-mixtral-8x7B-traces.zip -C ${SCRIPT_DIR}
rm ${SCRIPT_DIR}/nemo-chakra-mixtral-8x7B-traces.zip
