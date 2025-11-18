import subprocess
import time
import sys
import os

def run_test(commands):
    """
    Runs xv6 in qemu-nox, sends commands, and captures output.
    """
    print(f"Starting xv6 with commands: {commands}")
    
    # Start qemu-nox
    # using setsid to create a new session so we can kill the whole group later if needed
    process = subprocess.Popen(
        ["make", "qemu-nox"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=os.getcwd(),
        text=True,
        bufsize=0  # Unbuffered
    )

    output_log = []
    
    try:
        # Give it some time to boot
        time.sleep(2)
        
        # Send commands
        for cmd in commands:
            print(f"Sending: {cmd}")
            process.stdin.write(cmd + "\n")
            process.stdin.flush()
            time.sleep(1) # Wait for command to execute

        # Terminate QEMU (Ctrl-A X)
        print("Terminating QEMU...")
        time.sleep(3) # Let it run for a bit
        process.stdin.write("\x01x")
        process.stdin.flush()
        
        # Read remaining output
        time.sleep(1)
        process.terminate()
        
    except Exception as e:
        print(f"Error: {e}")
        process.kill()
        
    # Capture output
    stdout, stderr = process.communicate()
    
    print("\n--- XV6 Output ---")
    print(stdout)
    print("\n--- End Output ---")

if __name__ == "__main__":
    # Default test command
    cmds = ["ls", "echo Hello from Python"]
    if len(sys.argv) > 1:
        cmds = sys.argv[1:]
        
    run_test(cmds)
