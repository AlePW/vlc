/*****************************************************************************
 * threads.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: threads.h,v 1.18 2001/06/14 01:49:44 sam Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdio.h>

#if defined( PTH_INIT_IN_PTH_H )                                  /* GNU Pth */
#   include <pth.h>

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )  /* pthreads (like Linux & BSD) */
#   include <pthread.h>

#elif defined( HAVE_CTHREADS_H )                                  /* GNUMach */
#   include <cthreads.h>

#elif defined( HAVE_KERNEL_SCHEDULER_H )                             /* BeOS */
#   undef MAX
#   undef MIN
#   include <kernel/OS.h>
#   include <kernel/scheduler.h>
#   include <byteorder.h>

#elif defined( WIN32 )                        /* Win32 with MinGW32 compiler */
#   include <windows.h>
#   include <process.h>

#else
#   error no threads available on your system !

#endif

/*****************************************************************************
 * Constants
 *****************************************************************************
 * These constants are used by all threads in *_CreateThread() and
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *****************************************************************************/

/* Void status - this value can be used to make sure no operation is currently
 * in progress on the concerned thread in an array of recorded threads */
#define THREAD_NOP          0                            /* nothing happened */

/* Creation status */
#define THREAD_CREATE       10                     /* thread is initializing */
#define THREAD_START        11                          /* thread has forked */
#define THREAD_READY        19                            /* thread is ready */

/* Destructions status */
#define THREAD_DESTROY      20            /* destruction order has been sent */
#define THREAD_END          21        /* destruction order has been received */
#define THREAD_OVER         29             /* thread does not exist any more */

/* Error status */
#define THREAD_ERROR        30                           /* an error occured */
#define THREAD_FATAL        31  /* an fatal error occured - program must end */

/*****************************************************************************
 * Types definition
 *****************************************************************************/

#if defined( PTH_INIT_IN_PTH_H )
typedef pth_t            vlc_thread_t;
typedef pth_mutex_t      vlc_mutex_t;
typedef pth_cond_t       vlc_cond_t;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

#elif defined( HAVE_CTHREADS_H )
typedef cthread_t        vlc_thread_t;

/* Those structs are the ones defined in /include/cthreads.h but we need
 * to handle (*foo) where foo is a (mutex_t) while they handle (foo) where
 * foo is a (mutex_t*) */
typedef struct s_mutex {
    spin_lock_t held;
    spin_lock_t lock;
    char *name;
    struct cthread_queue queue;
} vlc_mutex_t;

typedef struct s_condition {
    spin_lock_t lock;
    struct cthread_queue queue;
    char *name;
    struct cond_imp *implications;
} vlc_cond_t;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
/* This is the BeOS implementation of the vlc threads, note that the mutex is
 * not a real mutex and the cond_var is not like a pthread cond_var but it is
 * enough for what wee need */

typedef thread_id vlc_thread_t;

typedef struct
{
    int32           init;
    sem_id          lock;
} vlc_mutex_t;

typedef struct
{
    int32           init;
    thread_id       thread;
} vlc_cond_t;

#elif defined( WIN32 )
typedef HANDLE      vlc_thread_t;
typedef HANDLE      vlc_mutex_t;
typedef HANDLE      vlc_cond_t; 
typedef unsigned (__stdcall *PTHREAD_START) (void *);

#endif

typedef void *(*vlc_thread_func_t)(void *p_data);

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

static __inline__ int  vlc_thread_create ( vlc_thread_t *, char *,
                                           vlc_thread_func_t, void * );
static __inline__ void vlc_thread_exit   ( void );
static __inline__ void vlc_thread_join   ( vlc_thread_t );

static __inline__ int  vlc_mutex_init    ( vlc_mutex_t * );
static __inline__ int  vlc_mutex_lock    ( vlc_mutex_t * );
static __inline__ int  vlc_mutex_unlock  ( vlc_mutex_t * );
static __inline__ int  vlc_mutex_destroy ( vlc_mutex_t * );

static __inline__ int  vlc_cond_init     ( vlc_cond_t * );
static __inline__ int  vlc_cond_signal   ( vlc_cond_t * );
static __inline__ int  vlc_cond_wait     ( vlc_cond_t *, vlc_mutex_t * );
static __inline__ int  vlc_cond_destroy  ( vlc_cond_t * );

#if 0
static __inline__ int  vlc_cond_timedwait( vlc_cond_t *, vlc_mutex_t *,
                                           mtime_t );
#endif

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
static __inline__ int vlc_thread_create( vlc_thread_t *p_thread,
                                         char *psz_name, vlc_thread_func_t func,
                                         void *p_data )
{
#if defined( PTH_INIT_IN_PTH_H )
    *p_thread = pth_spawn( PTH_ATTR_DEFAULT, func, p_data );
    return ( p_thread == NULL );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_create( p_thread, NULL, func, p_data );

#elif defined( HAVE_CTHREADS_H )
    *p_thread = cthread_fork( (cthread_fn_t)func, (any_t)p_data );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    *p_thread = spawn_thread( (thread_func)func, psz_name,
                              B_NORMAL_PRIORITY, p_data );
    return resume_thread( *p_thread );

#elif defined( WIN32 )
#if 0
    DWORD threadID;
    /* This method is not recommended when using the MSVCRT C library,
     * so we'll have to use _beginthreadex instead */
    *p_thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE) func, 
                             p_data, 0, &threadID);
#endif
    unsigned threadID;
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with memory
     * leaks and the signal function not working */
    *p_thread = (HANDLE)_beginthreadex(NULL, 0, (PTHREAD_START) func, 
                             p_data, 0, &threadID);
    
    return( *p_thread ? 0 : 1 );

#endif
}

/*****************************************************************************
 * vlc_thread_exit: terminate a thread
 *****************************************************************************/
static __inline__ void vlc_thread_exit( void )
{
#if defined( PTH_INIT_IN_PTH_H )
    pth_exit( 0 );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_exit( 0 );

#elif defined( HAVE_CTHREADS_H )
    int result;
    cthread_exit( &result );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    exit_thread( 0 );

#elif defined( WIN32 )
#if 0
    ExitThread( 0 );
#endif
    /* For now we don't close the thread handles (because of race conditions).
     * Need to be looked at. */
    _endthreadex(0);

#endif
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
static __inline__ void vlc_thread_join( vlc_thread_t thread )
{
#if defined( PTH_INIT_IN_PTH_H )
    pth_join( thread, NULL );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_join( thread, NULL );

#elif defined( HAVE_CTHREADS_H )
    cthread_join( thread );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32 exit_value;
    wait_for_thread( thread, &exit_value );

#elif defined( WIN32 )
    WaitForSingleObject( thread, INFINITE);

#endif
}

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_init( p_mutex );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_mutex_init( p_mutex, NULL );

#elif defined( HAVE_CTHREADS_H )
    mutex_init( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )

    /* check the arguments and whether it's already been initialized */
    if( p_mutex == NULL )
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init == 9999 )
    {
        return EALREADY;
    }

    p_mutex->lock = create_sem( 1, "BeMutex" );
    if( p_mutex->lock < B_NO_ERROR )
    {
        return( -1 );
    }

    p_mutex->init = 9999;
    return B_OK;

#elif defined( WIN32 )
    *p_mutex = CreateMutex(0,FALSE,0);
    return (*p_mutex?0:1);

#endif
}

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_lock( vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_acquire( p_mutex, TRUE, NULL );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_mutex_lock( p_mutex );

#elif defined( HAVE_CTHREADS_H )
    mutex_lock( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    status_t err;

    if( !p_mutex )
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init < 2000 )
    {
        return B_NO_INIT;
    }

    err = acquire_sem( p_mutex->lock );
    return err;

#elif defined( WIN32 )
    WaitForSingleObject( *p_mutex, INFINITE );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_unlock( vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_release( p_mutex );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_mutex_unlock( p_mutex );

#elif defined( HAVE_CTHREADS_H )
    mutex_unlock( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_mutex)
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init < 2000 )
    {
        return B_NO_INIT;
    }

    release_sem( p_mutex->lock );
    return B_OK;

#elif defined( WIN32 )
    ReleaseMutex( *p_mutex );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_destroy( vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )    
    return pthread_mutex_destroy( p_mutex );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex->init == 9999 )
    {
        delete_sem( p_mutex->lock );
    }

    p_mutex->init = 0;
    return B_OK;

#elif defined( WIN32 )
    CloseHandle(*p_mutex);
    return 0;

#endif    
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
static __inline__ int vlc_cond_init( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_init( p_condvar );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_init( p_condvar, NULL );

#elif defined( HAVE_CTHREADS_H )
    /* condition_init() */
    spin_lock_init( &p_condvar->lock );
    cthread_queue_init( &p_condvar->queue );
    p_condvar->name = 0;
    p_condvar->implications = 0;

    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init == 9999 )
    {
        return EALREADY;
    }

    p_condvar->thread = -1;
    p_condvar->init = 9999;
    return 0;

#elif defined( WIN32 )
    /* Create an auto-reset event. */
    *p_condvar = CreateEvent( NULL,   /* no security */
                              FALSE,  /* auto-reset event */
                              FALSE,  /* non-signaled initially */
                              NULL ); /* unnamed */

    return( *p_condvar ? 0 : 1 );
    
#endif
}

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
static __inline__ int vlc_cond_signal( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_notify( p_condvar, FALSE );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_signal( p_condvar );

#elif defined( HAVE_CTHREADS_H )
    /* condition_signal() */
    if ( p_condvar->queue.head || p_condvar->implications )
    {
        cond_signal( (condition_t)p_condvar );
    }
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init < 2000 )
    {
        return B_NO_INIT;
    }

    while( p_condvar->thread != -1 )
    {
        thread_info info;
        if( get_thread_info(p_condvar->thread, &info) == B_BAD_VALUE )
        {
            return 0;
        }

        if( info.state != B_THREAD_SUSPENDED )
        {
            /* The  waiting thread is not suspended so it could
             * have been interrupted beetwen the unlock and the
             * suspend_thread line. That is why we sleep a little
             * before retesting p_condver->thread. */
            snooze( 10000 );
        }
        else
        {
            /* Ok, we have to wake up that thread */
            resume_thread( p_condvar->thread );
            return 0;
        }
    }
    return 0;

#elif defined( WIN32 )
    /* Try to release one waiting thread. */
    PulseEvent ( *p_condvar );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
static __inline__ int vlc_cond_wait( vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_await( p_condvar, p_mutex, NULL );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_wait( p_condvar, p_mutex );

#elif defined( HAVE_CTHREADS_H )
    condition_wait( (condition_t)p_condvar, (mutex_t)p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( !p_mutex )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init < 2000 )
    {
        return B_NO_INIT;
    }

    /* The p_condvar->thread var is initialized before the unlock because
     * it enables to identify when the thread is interrupted beetwen the
     * unlock line and the suspend_thread line */
    p_condvar->thread = find_thread( NULL );
    vlc_mutex_unlock( p_mutex );
    suspend_thread( p_condvar->thread );
    p_condvar->thread = -1;

    vlc_mutex_lock( p_mutex );
    return 0;

#elif defined( WIN32 )
    /* Release the <external_mutex> here and wait for the event
     * to become signaled, due to <pthread_cond_signal> being
     * called. */
    vlc_mutex_unlock( p_mutex );

    WaitForSingleObject( *p_condvar, INFINITE );

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition
 *****************************************************************************/
static __inline__ int vlc_cond_destroy( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_destroy( p_condvar );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_condvar->init = 0;
    return 0;

#elif defined( WIN32 )
    CloseHandle( *p_condvar );
    return 0;

#endif    
}

