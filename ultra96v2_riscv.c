#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>      // dma_alloc_*, dma_free
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

static struct device* dma_dev = NULL;
static dma_addr_t dma_handle;
static void* cpu_addr = NULL;

static int Device_Open = 0;

#define CMA_MALLOC_DEVICE_FILENAME "rv64core_fpga"
#define REGION_SZ (448*1024*1024)

static int cma_malloc_mmap(struct file* fptr, struct vm_area_struct* vma){
  return 0;
};

static long cma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  if(__copy_to_user( (dma_addr_t*)arg, &dma_handle, sizeof(dma_handle) )) {
    return -EFAULT;
  }
  return 0;
}

static int cma_open(struct inode *inode_, struct file * file_) {
  if(Device_Open) {
    return -EBUSY;
  }
  Device_Open++;
  return 0;
}

static int cma_release(struct inode *inode_, struct file * file_) {
  Device_Open--;
  return 0;
}

static struct file_operations cma_malloc_fileops = {
    .owner            =   THIS_MODULE,
    .open             =   cma_open,
    .release          =   cma_release,
    .mmap             =   cma_malloc_mmap,
    .unlocked_ioctl   =   cma_ioctl
};

static struct miscdevice cma_malloc_miscdevice = {
    .minor           =   MISC_DYNAMIC_MINOR,
    .name            =   CMA_MALLOC_DEVICE_FILENAME,
    .fops            =   &cma_malloc_fileops,
    .mode            =   S_IRUGO | S_IWUGO,
};


static int __init fpga_init(void) {
  int ret;
  ret = misc_register(&cma_malloc_miscdevice);
  if(ret) {
    return -EIO;
  }
  dma_dev = cma_malloc_miscdevice.this_device;
  dma_dev->coherent_dma_mask = DMA_BIT_MASK(64);
  dma_dev->dma_mask = &dma_dev->coherent_dma_mask;  
  
  cpu_addr = dma_alloc_coherent(dma_dev, REGION_SZ, &dma_handle, GFP_KERNEL);
  if(!cpu_addr)
    return -ENOMEM;

  printk(KERN_NOTICE "linux virtual address = %lx, phys addr = %lx\n", cpu_addr, dma_handle);


  return 0; // success
}

static void __exit fpga_exit(void) {
  dma_free_coherent(dma_dev, REGION_SZ, cpu_addr, dma_handle);
  misc_deregister(&cma_malloc_miscdevice);
}

module_init(fpga_init);
module_exit(fpga_exit);
MODULE_LICENSE("GPL");
