import os
import sys

def run_command(command):
    result = os.system(command)
    if result != 0:
        print("Command '%s' failed with error code: %d" % (command, result))
        sys.exit(1)

def main(device_number):
    # Clean and make the module
    print("Cleaning and building the module...")
    run_command("make clean")
    run_command("make")

    # Remove the module if it already exists
    run_command("rmmod vegenere")

    # Insert the module
    print("Inserting the module...")
    run_command("insmod ./vegenere.o")

    # Remove the device node if it already exists
    run_command("rm -f /dev/vegenere")

    # Create the device node
    print("Creating the device node...")
    run_command("mknod /dev/vegenere c %s 0" % device_number)

    # Set permissions for the device node
    run_command("chmod 666 /dev/vegenere")

    # Run the test script
    print("Running the test script...")
    run_command("python test.py")

    # Remove the device node and module
    print("Removing the device node and module...")
    run_command("rm -f /dev/vegenere")
    run_command("rmmod vegenere")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python run_module.py <device_number>")
        sys.exit(1)
    device_number = sys.argv[1]
    main(device_number)
