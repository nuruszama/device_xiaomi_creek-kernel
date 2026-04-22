#!/bin/bash
echo "[+] Running Creek Hardware Seasoning..."

# 1. THE QCOM DEFS FIX
QCOM_DEFS="hardware/qcom-caf/common/qcom_defs.mk"
if [ -f "$QCOM_DEFS" ] && ! grep -q "UM 4.19 upgraded to UM 5.15" "$QCOM_DEFS"; then
    echo -e "\nifeq (\$(TARGET_KERNEL_VERSION),5.15)\n#UM 4.19 upgraded to UM 5.15\nUM_5_15_FAMILY := \$(UM_5_15_FAMILY) \$(UM_4_19_FAMILY)\nUM_4_19_FAMILY :=\nendif" >> "$QCOM_DEFS"
    echo "    [*] Applied 5.15 upgrade logic."
fi

# 2. THE MASS HEADER FIX
HEADER_PATH="device/xiaomi/creek-kernel/kernel-headers/usr/include"

TARGET_DIRS=(
    "hardware/qcom-caf/sm8550/audio/graphservices/ar_osal"
    "hardware/qcom-caf/sm8250/display/sde-drm"
    "hardware/qcom-caf/sm8250/display/gralloc"
)

for DIR in "${TARGET_DIRS[@]}"; do
    BP="$DIR/Android.bp"
    if [ -f "$BP" ]; then
        if ! grep -q "$HEADER_PATH" "$BP"; then
            echo "    [*] Patching $BP..."
            # Using a safer approach: find the line and insert after it
            # This avoids the \n issues in different sed versions
            sed -i "s|include_dirs: \[|include_dirs: \[ \n        \"$HEADER_PATH\",|g" "$BP"
            
            # Double check: if it failed to create a newline, fix it with a literal break
            if grep -q "\[ n" "$BP"; then
                 sed -i "s|\[ n|\[\n|g" "$BP"
            fi
        fi
    fi
done

echo "[+] Seasoning complete."
