# MSM7x01A
   zreladdr-$(CONFIG_ARCH_MSM7X01A)	:= 0x10008000
params_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10000100
initrd_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10800000

# MSM7x25
   zreladdr-$(CONFIG_ARCH_MSM7X25)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x0A000000

# MSM7x27
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x0A000000

# MSM7x27A
   zreladdr-$(CONFIG_ARCH_MSM7X27A)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X27A)	:= 0x00200100

# MSM8625
   zreladdr-$(CONFIG_ARCH_MSM8625)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM8625)	:= 0x00200100

# MSM7x30
   zreladdr-$(CONFIG_ARCH_MSM7X30)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x01200000

ifeq ($(CONFIG_MSM_SOC_REV_A),y)
# QSD8x50
   zreladdr-$(CONFIG_ARCH_QSD8X50)	:= 0x20008000
params_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x20000100
initrd_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x24000000
endif

# MSM8x60
   zreladdr-$(CONFIG_ARCH_MSM8X60)	:= 0x40208000

# MSM8960
   zreladdr-$(CONFIG_ARCH_MSM8960)	:= 0x80208000

# MSM8930
   zreladdr-$(CONFIG_ARCH_MSM8930)	:= 0x80208000

# APQ8064
   zreladdr-$(CONFIG_ARCH_APQ8064)	:= 0x80208000

# MSM8974
   zreladdr-$(CONFIG_ARCH_MSM8974)	:= 0x00008000
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-oslo-1-1.dtb
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-oslo-2-5.dtb
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-oslo-6-1.dtb
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-wolverine.dtb
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-wolverine-5.dtb
        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974pro-pm8941-wichita.dtb

# APQ8084
   zreladdr-$(CONFIG_ARCH_APQ8084)	:= 0x00008000
        dtb-$(CONFIG_ARCH_APQ8084)	+= apq8084-sim.dtb

# MSMKRYPTON
   zreladdr-$(CONFIG_ARCH_MSMKRYPTON)	:= 0x00008000
	dtb-$(CONFIG_ARCH_MSMKRYPTON)	+= msmkrypton-sim.dtb

# MSM9615
   zreladdr-$(CONFIG_ARCH_MSM9615)	:= 0x40808000

# MSM9625
   zreladdr-$(CONFIG_ARCH_MSM9625)	:= 0x00208000
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-rumi.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2.1-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2.1-cdp.dtb

# MSM8226
   zreladdr-$(CONFIG_ARCH_MSM8226)	:= 0x00008000
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sim.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-fluid.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-qrd-evt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-qrd-dvt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-720p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-1080p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-720p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-1080p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-qrd-evt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-qrd-dvt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-720p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-1080p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-720p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-1080p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-qrd.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-qrd-skug.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v1-qrd-skug-pvt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-720p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-1080p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-1080p-ext-buck-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-720p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-1080p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-1080p-ext-buck-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-qrd.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-qrd-skug.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-v2-qrd-skug-pvt.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-qrd-skuf.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-qrd-skuf.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v1-xpm.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v1-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v1-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-xpm.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-720p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-1080p-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-720p-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-1080p-mtp.dtb

# FSM9XXX
   zreladdr-$(CONFIG_ARCH_FSM9XXX)	:= 0x10008000
params_phys-$(CONFIG_ARCH_FSM9XXX)	:= 0x10000100
initrd_phys-$(CONFIG_ARCH_FSM9XXX)	:= 0x12000000

# FSM9900
   zreladdr-$(CONFIG_ARCH_FSM9900)	:= 0x00008000
        dtb-$(CONFIG_ARCH_FSM9900)	:= fsm9900-rumi.dtb
        dtb-$(CONFIG_ARCH_FSM9900)	:= fsm9900-sim.dtb

# MPQ8092
   zreladdr-$(CONFIG_ARCH_MPQ8092)	:= 0x00008000

# MSM8610
   zreladdr-$(CONFIG_ARCH_MSM8610)	:= 0x00008000
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-rumi.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-sim.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-qrd-skuaa.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-qrd-skuab.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-qrd-skuaa.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-qrd-skuab.dtb

# MSMSAMARIUM
   zreladdr-$(CONFIG_ARCH_MSMSAMARIUM)	:= 0x00008000
	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-sim.dtb
	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-rumi.dtb
