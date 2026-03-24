#!/bin/bash

# Ensure we are running as root to access /dev/input
if [ "$EUID" -ne 0 ]; then
  echo "Please run this configuration generator as root (sudo)."
  exit 1
fi

CONFIG_FILE="/etc/input-bridge.conf"
TEMP_FILE="/tmp/input-bridge-devices.tmp"

echo "Scanning for compatible input devices..."

# Extract device names using a quick dry-run of our tool, filtering for "Grabbed device" or "Skipping device" to get all potentials
# We use a slight hack here: we run dry run for 1 second in the background just to trigger the discovery phase
./build/input-bridge --dry-run > "$TEMP_FILE" 2>&1 &
DRY_PID=$!
sleep 0.5
kill "$DRY_PID" 2>/dev/null
wait "$DRY_PID" 2>/dev/null

echo ""
echo "Found the following devices:"
echo "------------------------------------------------"

# Parse the output to get unique device names
DEVICES=()
while IFS= read -r line; do
    # Extract the name between "device: " and " (/dev/input"
    if [[ "$line" =~ device:\ (.*)\ \(\/dev\/input\/event[0-9]+\) ]]; then
        name="${BASH_REMATCH[1]}"
        # Ensure name doesn't say "(not in config)" from skipping
        name=${name//\(not in config\):\ /}
        
        # Add if not already in array
        if [[ ! " ${DEVICES[*]} " =~ " ${name} " ]]; then
            DEVICES+=("$name")
        fi
    fi
done < "$TEMP_FILE"

rm -f "$TEMP_FILE"

if [ ${#DEVICES[@]} -eq 0 ]; then
    echo "No devices found."
    exit 0
fi

# Print them with numbers
for i in "${!DEVICES[@]}"; do
    echo "[$i] ${DEVICES[$i]}"
done
echo "------------------------------------------------"
echo ""

echo "Enter the numbers of the devices you want to grab, separated by spaces (e.g., '0 2 4'):"
read -p "> " selections

SELECTED_NAMES=()
for num in $selections; do
    if [[ "$num" =~ ^[0-9]+$ ]] && [ "$num" -ge 0 ] && [ "$num" -lt "${#DEVICES[@]}" ]; then
        SELECTED_NAMES+=("${DEVICES[$num]}")
    else
        echo "Warning: Ignored invalid selection '$num'"
    fi
done

if [ ${#SELECTED_NAMES[@]} -eq 0 ]; then
    echo "No valid devices selected. Configuration not updated."
    exit 0
fi

echo ""
echo "Writing the following devices to $CONFIG_FILE:"
> "$CONFIG_FILE"
for name in "${SELECTED_NAMES[@]}"; do
    echo "- $name"
    echo "$name" >> "$CONFIG_FILE"
done

echo ""
echo "Configuration saved successfully!"
