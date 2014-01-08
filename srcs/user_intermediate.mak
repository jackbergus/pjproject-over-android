# I flag minimali di CFLAGS sono: -fno-PIC -fomit-frame-pointer -Wno-sign-compare 
# -march=armv7-a -mfloat-abi=softfp -mfp=vfp -D__ARM_ARCH_7__
export CFLAGS += -Os -O2 \
		 -mfloat-abi=softfp -mfp=vfp -mfpu=vfpv3-d16 \
		 -fpic  -funwind-tables -fomit-frame-pointer -fstack-protector \
		 	-ffunction-sections -fno-strict-aliasing  \
		 -mthumb-interwork -fno-exceptions \
		 -Wno-psabi -Wno-sign-compare -Wa,--noexecstack  \
		 -D__ARM_ARCH_7__ -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ \
		 	-D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -DANDROID -DNDEBUG\
		 	-g \
		 

#Questo flag è proprio di C++ per supc++
export CPPFLAGS += -fno-rtti

export LDFLAGS += -lOpenSLES 

# Voglio definire l'entry point _start della mia applicazione, mancante nella 
# relase ufficiale di Android. Questa informazione verrà accodata a tutti gli 
# altri .o predefiniti dell'applicazione. In quanto questo verrà replicato una 
# volta sola (al contrario di -ldflags), definiamo qui che solamente la libc 
# deve essere caricata staticamente. Inoltre in questo modo garantisco che tale 
# linking avvenga solamente per la compilazione delle applicazioni
export PJ_LDLIBS += /path/to/crt0.o  -lc -lgcc 
