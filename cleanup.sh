make clean
make mrproper

rm arch/arm64/boot/dts/*/*.dtb*
rm arch/arm64/boot/dts/*/*/*/*.dtb*
rm arch/arm64/boot/dts/vendor/qcom/yupik.dtb*
rm arch/arm64/crypto/sha256-core.S
rm -rf arch/arm64/include/generated/
rm arch/arm64/kernel/vdso/vdso.lds
rm arch/arm64/kernel/vdso/vdso.so
rm arch/arm64/kernel/vdso/vdso.so.dbg
rm arch/arm64/kernel/vmlinux.lds
rm drivers/cpuidle/.tmp_cpuidle.ver
rm kernel/.tmp_capability.ver

git checkout -- scripts/
git checkout -- security/
