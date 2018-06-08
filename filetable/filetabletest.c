#include <stdio.h>
#include <stdlib.h>

#include "filetable.h"


int main(const int argc, char *argv[])
{
	filetable_print(NULL);
	FileTable *ft = filetable_init();
	filetable_print(ft);
	filetable_insert(ft, "/Usrs/", 10, 100, "0.0.0.0", 5000);
	filetable_print(ft);
	filetable_addPeer(ft, "/Usrs/", "0.0.0.1", 5000);
	filetable_print(ft);
	filetable_insert(ft, "/Usrs/b", 10, 100, "0.0.0.0", 5000);
	filetable_print(ft);
	filetable_updateMod(ft, "/Usrs/", 50, 500, "0.0.0.1", 5000);
	filetable_print(ft);
	filetable_entryprint(filetable_getEntry(ft, "/Usrs/"));
	filetable_insert(ft, "/Usrs/a", 10, 100, "0.0.0.0", 5000);
	filetable_print(ft);
	filetable_remove(ft, "/Usrs/");
	filetable_print(ft);
	filetable_destroy(ft);
}
