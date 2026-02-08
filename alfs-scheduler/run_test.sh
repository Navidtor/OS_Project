#!/bin/bash
# ALFS Test Runner
# Runs the test server and scheduler together

SOCKET_PATH="${1:-event.socket}"
INPUT_FILE="${2:-tests/sample_input.json}"
CPUS="${3:-4}"

echo "========================================"
echo "ALFS Test Runner"
echo "========================================"
echo "Socket: $SOCKET_PATH"
echo "Input:  $INPUT_FILE"
echo "CPUs:   $CPUS"
echo "========================================"

# Clean up any old socket
rm -f "$SOCKET_PATH"

# Start the test server in background
echo "Starting test server..."
python3 tests/test_server.py "$SOCKET_PATH" "$INPUT_FILE" &
SERVER_PID=$!

# Give it a moment to create the socket
sleep 1

# Run the scheduler
echo "Starting ALFS scheduler..."
./alfs_scheduler -s "$SOCKET_PATH" -c "$CPUS" -m

# Wait for server to complete
wait $SERVER_PID

echo ""
echo "Test complete!"
