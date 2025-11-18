use std::{
    sync::{
        Arc,
        atomic::{AtomicU32, AtomicUsize, Ordering},
    },
    thread,
    time::Instant,
};

use crate::mmap::Mmap;

mod mmap;

fn get_page_size() -> std::io::Result<usize> {
    let ret = unsafe { libc::sysconf(libc::_SC_PAGESIZE) };
    usize::try_from(ret).map_err(|_err| std::io::Error::last_os_error())
}

unsafe fn madvise(addr: *mut libc::c_void, len: usize, advice: libc::c_int) -> std::io::Result<()> {
    let ret = unsafe { libc::madvise(addr, len, advice) };
    if ret == -1 {
        return Err(std::io::Error::last_os_error());
    }
    Ok(())
}

const EXPERIMENT_1_COUNT: usize = 8192;

fn experiment_1_tokio() {
    let runtime = tokio::runtime::Builder::new_multi_thread().build().unwrap();

    struct Shared {
        start_barrier: tokio::sync::Barrier,
        start_notify: tokio::sync::Notify,
        remaining: AtomicUsize,
        end_notify: tokio::sync::Notify,
    }

    let shared = Arc::new(Shared {
        start_barrier: tokio::sync::Barrier::new(EXPERIMENT_1_COUNT + 1),
        start_notify: tokio::sync::Notify::new(),
        remaining: AtomicUsize::new(EXPERIMENT_1_COUNT),
        end_notify: tokio::sync::Notify::new(),
    });

    let page_size = get_page_size().unwrap();
    let mut mmap = Mmap::new_anonymous(libc::PROT_READ, EXPERIMENT_1_COUNT * page_size).unwrap();
    unsafe { madvise(mmap.as_mut_ptr().cast(), mmap.len(), libc::MADV_NOHUGEPAGE).unwrap() };

    while !mmap.is_empty() {
        let (page, rest) = unsafe { mmap.split_at_unchecked(page_size) };
        let shared_ = Arc::clone(&shared);
        runtime.spawn(async move {
            let start_notified = shared_.start_notify.notified();
            shared_.start_barrier.wait().await;
            start_notified.await;
            unsafe { core::ptr::read_volatile(page.as_ptr()) };
            if shared_.remaining.fetch_sub(1, Ordering::Relaxed) == 1 {
                shared_.end_notify.notify_one();
            }
        });
        mmap = rest;
    }

    runtime.block_on(async {
        shared.start_barrier.wait().await;
        let start = Instant::now();
        shared.start_notify.notify_waiters();
        shared.end_notify.notified().await;
        let end = Instant::now();
        println!("Tokio: {}ns", end.duration_since(start).as_nanos());
    });
}

fn experiment_1_threads() {
    struct Shared {
        start_barrier: std::sync::Barrier,
        start: AtomicU32,
        remaining: AtomicUsize,
        end: AtomicU32,
    }

    let shared = Arc::new(Shared {
        start_barrier: std::sync::Barrier::new(EXPERIMENT_1_COUNT + 1),
        start: AtomicU32::new(0),
        remaining: AtomicUsize::new(EXPERIMENT_1_COUNT),
        end: AtomicU32::new(0),
    });

    let page_size = get_page_size().unwrap();
    let mut mmap = Mmap::new_anonymous(libc::PROT_READ, EXPERIMENT_1_COUNT * page_size).unwrap();
    unsafe { madvise(mmap.as_mut_ptr().cast(), mmap.len(), libc::MADV_NOHUGEPAGE).unwrap() };

    thread::scope(|scope| {
        while !mmap.is_empty() {
            let (page, rest) = unsafe { mmap.split_at_unchecked(page_size) };
            let shared_ = Arc::clone(&shared);
            scope.spawn(move || {
                shared_.start_barrier.wait();
                while shared_.start.load(Ordering::Relaxed) == 0 {
                    atomic_wait::wait(&shared_.start, 0);
                }
                unsafe { core::ptr::read_volatile(page.as_ptr()) };
                if shared_.remaining.fetch_sub(1, Ordering::Relaxed) == 1 {
                    shared_.end.store(1, Ordering::Relaxed);
                    atomic_wait::wake_one(&shared_.end);
                }
            });
            mmap = rest;
        }

        shared.start_barrier.wait();
        let start = Instant::now();
        shared.start.store(1, Ordering::Relaxed);
        atomic_wait::wake_all(&shared.start);
        while shared.end.load(Ordering::Relaxed) == 0 {
            atomic_wait::wait(&shared.end, 0);
        }
        let end = Instant::now();
        println!("Threaded: {}ns", end.duration_since(start).as_nanos());
    });
}

fn main() {
    experiment_1_tokio();
    experiment_1_threads();
}
