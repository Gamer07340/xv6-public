#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int uid, gid;
  int ret_uid, ret_gid;

  printf(1, "Debug SU starting...\n");
  printf(1, "Initial UID: %d, GID: %d\n", getuid(), getgid());

  if(argc < 3){
    printf(1, "Usage: debug_su <uid> <gid>\n");
    exit();
  }

  uid = atoi(argv[1]);
  gid = atoi(argv[2]);

  printf(1, "Attempting to set UID to %d and GID to %d\n", uid, gid);

  ret_gid = setgid(gid);
  printf(1, "setgid(%d) returned: %d\n", gid, ret_gid);

  ret_uid = setuid(uid);
  printf(1, "setuid(%d) returned: %d\n", uid, ret_uid);

  printf(1, "Final UID: %d, GID: %d\n", getuid(), getgid());

  if(ret_gid < 0 || ret_uid < 0){
      printf(1, "FAILURE: setuid or setgid failed.\n");
  } else {
      printf(1, "SUCCESS: setuid and setgid succeeded.\n");
  }

  exit();
}
