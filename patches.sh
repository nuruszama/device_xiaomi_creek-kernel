#!/bin/bash
echo "[+] Running Creek Hardware Seasoning..."

# 1. THE QCOM DEFS FIX
QCOM_DEFS="hardware/qcom-caf/common/qcom_defs.mk"
if [ -f "$QCOM_DEFS" ] && ! grep -q "UM 4.19 upgraded to UM 5.15" "$QCOM_DEFS"; then
    echo -e "\nifeq (\$(TARGET_KERNEL_VERSION),5.15)\n#UM 4.19 upgraded to UM 5.15\nUM_5_15_FAMILY := \$(UM_5_15_FAMILY) \$(UM_4_19_FAMILY)\nUM_4_19_FAMILY :=\nendif" >> "$QCOM_DEFS"
    echo "    [*] Applied 5.15 upgrade logic."
fi

# 2. HEADER LIB FIX (SAFE METHOD)
TARGET_BPS=(
    "hardware/qcom-caf/sm8550/audio/graphservices/ar_osal/Android.bp"
    "hardware/qcom-caf/sm8250/display/sde-drm/Android.bp"
    "hardware/qcom-caf/sm8250/display/gralloc/Android.bp"
)

for BP in "${TARGET_BPS[@]}"; do
    if [ -f "$BP" ]; then
        if ! grep -q "extra_kernel_headers" "$BP"; then
            echo "    [*] Injecting extra_kernel_headers into $BP..."

            # Insert after qti_audio_kernel_uapi if present
            if grep -q '"qti_audio_kernel_uapi"' "$BP"; then
                sed -i '/"qti_audio_kernel_uapi"/a\        "extra_kernel_headers",' "$BP"

            # Fallback: insert after audio_kernel_headers
            elif grep -q '"audio_kernel_headers"' "$BP"; then
                sed -i '/"audio_kernel_headers"/a\        "extra_kernel_headers",' "$BP"

            else
                echo "    [!] Skipped $BP (no known header_libs anchor found)"
            fi
        else
            echo "    [=] Already patched: $BP"
        fi
    fi
done

echo "[+] Seasoning complete."
