#	LinBPQ Makefile

#	To exclude i2c support run make noi2c

OBJS = ARDOP.o Dragon.o IPCode.o FLDigi.o linether.o TNCEmulators.o CMSAuth.o APRSCode.o BPQtoAGW.o KAMPactor.o AEAPactor.o HALDriver.o MULTIPSK.o BBSHTMLConfig.o ChatHTMLConfig.o HTMLCommonCode.o BBSUtilities.o bpqaxip.o BPQINP3.o BPQNRR.o cMain.o Cmd.o CommonCode.o compatbits.o config.o datadefs.o FBBRoutines.o HFCommon.o Housekeeping.o HTTPcode.o kiss.o L2Code.o L3Code.o L4Code.o lzhuf32.o MailCommands.o MailDataDefs.o LinBPQ.o MailRouting.o MailTCP.o MBLRoutines.o md5.o Moncode.o NNTPRoutines.o RigControl.o TelnetV6.o WINMOR.o TNCCode.o UZ7HODrv.o WPRoutines.o SCSTrackeMulti.o SCSPactor.o SCSTracker.o HanksRT.o  UIRoutines.o AGWAPI.o AGWMoncode.o

# Configuration:

CC = gcc
		                       
all: CFLAGS = -DLINBPQ -MMD -g
all: linbpq


noi2c: CFLAGS = -DLINBPQ -MMD -DNOI2C -g
noi2c: linbpq

	
linbpq: $(OBJS)
	gcc $(OBJS) -Xlinker -Map=output.map -lrt -lm -lpthread -lconfig -lpcap -o linbpq
	sudo setcap "CAP_NET_ADMIN=ep CAP_NET_RAW=ep CAP_NET_BIND_SERVICE=ep" linbpq		

-include *.d

clean :
	rm linbpq $(OBJS)

