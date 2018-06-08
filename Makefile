MAKE = make

############## default: make all libs and programs ##########
all:
	$(MAKE) -C upload_download
	$(MAKE) -C monitor lib
	$(MAKE) -C peer
	$(MAKE) -C filetable
	$(MAKE) -C messaging
	$(MAKE) -C tracker

tracker:
	$(MAKE) -C monitor lib
	$(MAKE) -C filetable
	$(MAKE) -C messaging
	$(MAKE) -C tracker

.PHONY: all tracker clear

############## clean  ##########
clean:
	rm -rf *~ *.o *.dSYM .DS_Store
	$(MAKE) -C monitor clean
	$(MAKE) -C tracker clean
	$(MAKE) -C peer clean
	$(MAKE) -C upload_download clean
	$(MAKE) -C filetable clean
	$(MAKE) -C messaging clean
