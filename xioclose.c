/* source: xioclose.c */
/* Copyright Gerhard Rieger 2001-2008 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended close function */


#include "xiosysincludes.h"
#include "xioopen.h"
#include "xiolockfile.h"

#include "xio-termios.h"


/* close the xio fd; must be valid and "simple" (not dual) */
int xioclose1(struct single *pipe) {

   if (pipe->tag == XIO_TAG_INVALID) {
      Notice("xioclose1(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

   switch (pipe->howtoclose) {

#if WITH_READLINE
   case XIOCLOSE_READLINE:
      Write_history(pipe->para.readline.history_file);
      /*xiotermios_setflag(pipe->fd, 3, ECHO|ICANON);*/	/* error when pty closed */
      break;
#endif /* WITH_READLINE */

#if WITH_OPENSSL
   case XIOCLOSE_OPENSSL:
      if (pipe->para.openssl.ssl) {
	 /* e.g. on TCP connection refused, we do not yet have this set */
	 sycSSL_shutdown(pipe->para.openssl.ssl);
	 sycSSL_free(pipe->para.openssl.ssl);
	 pipe->para.openssl.ssl = NULL;
      }
      Close(pipe->fd1);  pipe->fd1 = -1;
      Close(pipe->fd2);  pipe->fd2 = -1;
      if (pipe->para.openssl.ctx) {
	 sycSSL_CTX_free(pipe->para.openssl.ctx);
	 pipe->para.openssl.ctx = NULL;
      }
      break;
#endif /* WITH_OPENSSL */

#if WITH_TERMIOS
   if (pipe->ttyvalid) {
      if (Tcsetattr(pipe->fd1, 0, &pipe->savetty) < 0) {
	 Warn2("cannot restore terminal settings on fd %d: %s",
	       pipe->fd1, strerror(errno));
      }
   }
#endif /* WITH_TERMIOS */

   case XIOCLOSE_SIGTERM:
      if (pipe->child.pid > 0) {
	 if (Kill(pipe->child.pid, SIGTERM) < 0) {
	    Msg2(errno==ESRCH?E_INFO:E_WARN, "kill(%d, SIGTERM): %s",
		 pipe->child.pid, strerror(errno));
	 }
      }
      break;
   case XIOCLOSE_CLOSE_SIGTERM:
      if (pipe->child.pid > 0) {
	    if (Kill(pipe->child.pid, SIGTERM) < 0) {
	       Msg2(errno==ESRCH?E_INFO:E_WARN, "kill(%d, SIGTERM): %s",
		    pipe->child.pid, strerror(errno));
	    }
      }
      /*PASSTHROUGH*/
   case XIOCLOSE_CLOSE:
      if (pipe->fd1 >= 0) {
	 if (Close(pipe->fd1) < 0) {
	    Info2("close(%d): %s", pipe->fd1, strerror(errno));
	 }
      }
      break;

   case XIOCLOSE_SLEEP_SIGTERM:
      Sleep(1);
      if (pipe->child.pid > 0) {
	    if (Kill(pipe->child.pid, SIGTERM) < 0) {
	       Msg2(errno==ESRCH?E_INFO:E_WARN, "kill(%d, SIGTERM): %s",
		    pipe->child.pid, strerror(errno));
	    }
      }
      break;

   case XIOCLOSE_NONE:
      break;

   default:
      Error2("xioclose(): bad end action 0x%x on 0x%x", pipe->howtoclose, pipe);
      break;
   }

   /* unlock */
   if (pipe->havelock) {
      xiounlock(pipe->lock.lockfile);
      pipe->havelock = false;
   }      
   if (pipe->opt_unlink_close && pipe->unlink_close) {
      if (Unlink(pipe->unlink_close) < 0) {
	 Info2("unlink(\"%s\"): %s", pipe->unlink_close, strerror(errno));
      }
      free(pipe->unlink_close);
   }

   pipe->tag = XIO_TAG_INVALID;
   return 0;	/*! */
}


/* close the xio fd */
int xioclose(xiofile_t *file) {
   xiofile_t *xfd = file;
   int result;

   if (file->tag == XIO_TAG_INVALID) {
      Error("xioclose(): invalid file descriptor");
      errno = EINVAL;
      return -1;
   }

   if (file->tag == XIO_TAG_DUAL) {
      result  = xioclose1(file->dual.stream[0]);
      result |= xioclose1(file->dual.stream[1]);
      file->tag = XIO_TAG_INVALID;
   } else {
      result = xioclose1(&file->stream);
   }
   if (xfd->stream.subthread != 0) {
      Pthread_join(xfd->stream.subthread, NULL);
   }
   return result;
}
