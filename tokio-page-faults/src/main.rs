use std::ptr;

pub struct PageFaultGenerator {
    mmap: memmap2::MmapMut,
    page_size: usize,
    offset: usize,
}

pub struct CapacityExceeded;

impl PageFaultGenerator {
    pub fn new(capacity: usize) -> std::io::Result<Self> {
        let ret = unsafe { libc::sysconf(libc::_SC_PAGESIZE) };
        let page_size = usize::try_from(ret).map_err(|_err| std::io::Error::last_os_error())?;
        let len = capacity * page_size;
        let mmap = memmap2::MmapOptions::new().len(len).map_anon()?;
        Ok(Self {
            mmap,
            page_size,
            offset: 0,
        })
    }

    pub fn generate_page_fault(&mut self) -> Result<(), CapacityExceeded> {
        if self.offset >= self.mmap.len() {
            return Err(CapacityExceeded);
        }
        let page = unsafe { self.mmap.as_ptr().add(self.offset) };
        unsafe { ptr::read_volatile(page) };
        self.offset += self.page_size;
        Ok(())
    }
}

fn main() {
    let runtime = tokio::runtime::Builder::new_multi_thread().build().unwrap();
    const COUNT: usize = 8192;
    let mut generator = PageFaultGenerator::new(COUNT).unwrap();
    runtime.block_on(async move { while generator.generate_page_fault().is_ok() {} });
}
