#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

/* Read all available inotify events from the file descriptor `fd`.
   wd: the table of watch descriptors for the directories in argv.
   argc: the length of wd and argv.
   argv: the list of watched directories.
*/

// Entry 0 of wd and argv is unused.

static void
handle_events(int fd, int *wd, int argc, char* argv[])
{
  char buf[4096];
  const struct inotify_event *event;
  int i;
  ssize_t len;
  char *ptr;

  for (;;) {
    // Read some events from `fd`.
    len = read(fd, buf, sizeof buf);
    if (len == -1 && errno != EAGAIN) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    /* If the nonblocking read() found no events to read, then
       it returns -1 with errno set to EAGAIN. In that case,
       we exit the loop. */
    if (len <= 0)
      break;

    // Loop over events.
    for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
      event = (const struct inotify_event *) ptr;

      // Print event type.
      if (event->mask & IN_OPEN)
	printf("IN_OPEN: ");
      if (event->mask & IN_CLOSE_NOWRITE)
	printf("IN_CLOSE_NOWRITE: ");
      if (event->mask & IN_CLOSE_WRITE)
	printf("IN_CLOSE_WRITE: ");
      if (event->mask & IN_MOVED_FROM)
	printf("IN_MOVED_FROM: ");
      if (event->mask & IN_MOVED_TO)
	printf("IN_MOVED_TO: ");

      // Print the name of the watched directory.
      for (i = 1; i < argc; ++i) {
	if (wd[i] == event->wd) {
	  printf("%s/", argv[i]);
	  break;
	}
      }

      // Print the name of the file.
      if (event->len)
	printf("%s", event->name);

      // Print type of filesystem object.
      if (event->mask & IN_ISDIR)
	printf(" [directory]\n");
      else
	printf(" [file]\n");
    }
  }
}

int
main(int argc, char* argv[])
{
  char buf;
  int fd, i, poll_num;
  int *wd;
  nfds_t nfds;
  struct pollfd fds[2];

  if (argc < 2) {
    printf("Usage: %s PATH [PATH ...]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  printf("Press ENTER key to exit.\n");

  /* Create the file descriptor for accessing the inotify API.
     IN_NONBLOCK: Set the O_NONBLOCK file status flag on the
     new open file descriptor.
  */
  fd = inotify_init1(IN_NONBLOCK);
  if (fd == -1) {
    perror("inotify_init1");
    exit(EXIT_FAILURE);
  }

  // Allocate memory for watch descriptors.
  wd = calloc(argc, sizeof(int));
  if (wd == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  /* Mark directories for events
     - file was opened
     - file was closed
  */
  for (i = 1; i < argc; i++) {
    wd[i] = inotify_add_watch(fd, argv[i], IN_OPEN | IN_CLOSE | IN_MOVE);
    if (wd[i] == -1) {
      fprintf(stderr, "Cannot watch '%s'\n", argv[i]);
      perror("inotify_add_watch");
      exit(EXIT_FAILURE);
    }
  }

  /* nfds: Number of file descriptors, we use 2 file descriptors:
     1- Console input 
     2- Inotify input
  */
  nfds = 2;

  // Console input.
  fds[0].fd = STDIN_FILENO;
  // POLLIN: There's data to read.
  fds[0].events = POLLIN;

  // Inotify input.
  fds[1].fd = fd;
  fds[1].events = POLLIN;

  // Wait for events and/or terminal input.
  printf("Listening for events.\n");
  
  for(;;) {
    poll_num = poll(fds, nfds, -1);
    if (poll_num == -1) {
      // EINTR: Received an interrupt signal.
      if (errno == EINTR)
	continue;
      perror("poll");
      exit(EXIT_FAILURE);
    }

    if (poll_num > 0) {

      /* revents: returned events, filled by the kernel with Events that
	 actually occured. */
      if (fds[0].revents & POLLIN) {

	// Console input is available. Empty stdin and quit.
	while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
	  continue;
	break;
      }

      if (fds[1].revents & POLLIN) {
	// Inotify events are available.
	handle_events(fd, wd, argc, argv);
      }
    }
  }

  printf("Listening for events stopped.\n");

  // Close inotify file descriptor.
  close(fd);
  free(wd);
  exit(EXIT_SUCCESS);
}
