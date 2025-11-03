use std::{
    sync::{
        Arc,
        atomic::{AtomicUsize, Ordering},
    },
    time::Instant,
};

use tokio::sync::{Barrier, Notify};

use crate::mmap::Mmap;

mod mmap;

fn get_page_size() -> std::io::Result<usize> {
    let ret = unsafe { libc::sysconf(libc::_SC_PAGESIZE) };
    usize::try_from(ret).map_err(|_err| std::io::Error::last_os_error())
}

fn main() {
    let runtime = tokio::runtime::Builder::new_multi_thread().build().unwrap();
    const COUNT: usize = 65536;
    let page_size = get_page_size().unwrap();
    let mut mmap = Mmap::new_anonymous(libc::PROT_READ, COUNT * page_size).unwrap();
    let start_barrier = Arc::new(Barrier::new(COUNT + 1));
    let remaining = Arc::new(AtomicUsize::new(COUNT));
    let done_notify = Arc::new(Notify::new());
    while !mmap.is_empty() {
        let (page, rest) = unsafe { mmap.split_at_unchecked(page_size) };
        let start_barrier_ = start_barrier.clone();
        let remaining_ = remaining.clone();
        let done_notify_ = done_notify.clone();
        runtime.spawn(async move {
            start_barrier_.wait().await;
            unsafe { core::ptr::read_volatile(page.as_ptr()) };
            if remaining_.fetch_sub(1, Ordering::Relaxed) == 1 {
                done_notify_.notify_one();
            }
        });
        mmap = rest;
    }
    runtime.block_on(async {
        let start = Instant::now();
        start_barrier.wait().await;
        done_notify.notified().await;
        let end = Instant::now();
        println!("{}ns", end.duration_since(start).as_nanos());
    });
}
