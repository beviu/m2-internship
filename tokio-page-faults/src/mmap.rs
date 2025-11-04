use std::{
    io,
    ops::{Deref, DerefMut},
};

pub struct Mmap {
    ptr: *mut libc::c_void,
    len: usize,
}

impl Mmap {
    pub fn new_anonymous(prot: i32, len: usize) -> std::io::Result<Self> {
        let ret = unsafe {
            libc::mmap(
                core::ptr::null_mut(),
                len,
                prot,
                libc::MAP_ANON | libc::MAP_PRIVATE,
                -1,
                0,
            )
        };
        if ret == libc::MAP_FAILED {
            return Err(io::Error::last_os_error());
        }
        let ptr = ret as *mut libc::c_void;
        Ok(Self { ptr, len })
    }

    pub unsafe fn split_at_unchecked(mut self, mid: usize) -> (Self, Self) {
        let left = Self {
            ptr: self.ptr,
            len: mid,
        };
        self.ptr = unsafe { self.ptr.add(mid) };
        self.len -= mid;
        (left, self)
    }
}

unsafe impl Send for Mmap {}

impl Deref for Mmap {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        unsafe { core::slice::from_raw_parts(self.ptr.cast(), self.len) }
    }
}

impl DerefMut for Mmap {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { core::slice::from_raw_parts_mut(self.ptr.cast(), self.len) }
    }
}

impl Drop for Mmap {
    fn drop(&mut self) {
        unsafe {
            libc::munmap(self.ptr, self.len);
        }
    }
}
