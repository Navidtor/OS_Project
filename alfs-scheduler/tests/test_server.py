#!/usr/bin/env python3
"""
ALFS Test Server
Simulates the tester that sends events and receives scheduler decisions.

Usage:
    python3 test_server.py [socket_path] [input_file]
    
Example:
    python3 tests/test_server.py event.socket tests/sample_input.json
"""

import socket
import os
import sys
import json
import time

DEFAULT_SOCKET = "event.socket"

def create_socket(socket_path):
    """Create a Unix Domain Socket server"""
    # Remove existing socket file
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.bind(socket_path)
    sock.listen(1)
    
    print(f"Test server listening on {socket_path}")
    return sock

def send_timeframe(conn, timeframe):
    """Send a single timeframe to the scheduler"""
    json_str = json.dumps(timeframe) + "\n"
    conn.sendall(json_str.encode('utf-8'))
    print(f"Sent vtime={timeframe['vtime']}: {len(timeframe['events'])} events")

def receive_tick(conn):
    """Receive a scheduler tick response"""
    buffer = b""
    while True:
        data = conn.recv(4096)
        if not data:
            return None
        buffer += data
        if b"\n" in buffer:
            break
    
    line = buffer.split(b"\n")[0]
    return json.loads(line.decode('utf-8'))

def run_test(socket_path, input_file):
    """Run the test with the given input file"""
    # Load input events
    with open(input_file, 'r') as f:
        timeframes = json.load(f)
    
    print(f"Loaded {len(timeframes)} timeframes from {input_file}")
    
    # Create socket and wait for connection
    server_sock = create_socket(socket_path)
    print("Waiting for scheduler to connect...")
    
    conn, addr = server_sock.accept()
    print("Scheduler connected!")
    
    try:
        results = []
        
        for tf in timeframes:
            # Send timeframe
            send_timeframe(conn, tf)
            
            # Receive response
            tick = receive_tick(conn)
            if tick:
                results.append(tick)
                schedule_str = ", ".join(tick['schedule'])
                print(f"  Response vtime={tick['vtime']}: [{schedule_str}]")
                
                if 'meta' in tick:
                    meta = tick['meta']
                    print(f"    Preemptions: {meta.get('preemptions', 0)}, "
                          f"Migrations: {meta.get('migrations', 0)}")
            else:
                print("  No response received")
        
        # Print summary
        print("\n" + "="*60)
        print("TEST COMPLETE")
        print("="*60)
        print(f"Total timeframes: {len(timeframes)}")
        print(f"Responses received: {len(results)}")
        
        # Output results to file
        output_file = input_file.replace(".json", "_output.json")
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"Results written to: {output_file}")
        
    finally:
        conn.close()
        server_sock.close()
        os.unlink(socket_path)

def main():
    socket_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET
    input_file = sys.argv[2] if len(sys.argv) > 2 else "tests/sample_input.json"
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    run_test(socket_path, input_file)

if __name__ == "__main__":
    main()
