#include "../CMASI/lmcp.h"
#include <stdio.h>


int main(int argc, char *argv[])
{
  
  FILE *file;
  char *buffer;
  unsigned long fileLen;
  lmcp_object * mc = NULL;

  if(argc < 1 || argc > 2) {
    fprintf(stderr,
	    "This program takes exactly one argument pointing to a mission command file.");
    return -1;
  }

  /* Open file */
  file = fopen(argv[1], "rb");
  if (!file)
    {
      fprintf(stderr, "Unable to open file %s", argv[1]);
      return -1;
    }
	
  /* Get file length */
  fseek(file, 0, SEEK_END);
  fileLen=ftell(file);
  fseek(file, 0, SEEK_SET);

  /* Allocate memory */
  buffer=(char *)malloc(fileLen+1);
  if (!buffer)
    {
      fprintf(stderr, "Memory error!");
      fclose(file);
      return -1;
    }

  /* Read file contents into buffer */
  fread(buffer, fileLen, 1, file);
  fclose(file);

  char * tmpbuffer = buffer;

  if(lmcp_process_msg((uint8_t **)&tmpbuffer,fileLen*20,&mc) == -1) {
    fprintf(stderr, "Failed to parse mission command\n");
    return -1;
  }

  fprintf(stderr, "Successfully read mission command.");

  lmcp_pp(mc);

	
  free(buffer);

  return 0;
}
