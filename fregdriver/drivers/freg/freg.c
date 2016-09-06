#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "freg.h"

static int freg_major = 0;	//主设备号
static int freg_minor = 0;	//从设备号

static struct class* freg_class = NULL;	//设备类别
static struct fake_reg_dev* freg_dev = NULL;	//设备

//传统设备文件操作方法
static init freg_open(struct inode* inode, struct file* filp);
static init freg_release(struct inode* inode, struct file* filp);
static ssize_t freg_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t freg_write(struct file* filp,const char __user *buf,size_t count,loff_t* f_pos);


static struct file_operations freg_fops = {
	.owner = THIS_MODULE,
	.open = freg_open,
	.release = freg_release,
	.read = freg_read,
	.wirte = freg_write,
};

/*devfs文件系统的设备属性操作方法*/
static ssize_t freg_val_show(struct device* dev,struct device_attribute* attr, char* buf);
static ssize_t freg_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

/*devfs文件系统的设备属性*/
static DEVICE_ATTR(val,S_IRUGO | S_IWUSR,freg_val_show,freg_val_store);



/*打开设备方法*/
static init freg_open(struct inode* inode, struct file* filp){
	struct fake_reg_dev* dev;

	/*将自定义设备结构题保存在文件指针的私有数据域中，以便访问设备时可以直接拿来用*/
	dev = container_of(inode->i_cdev,struct fake_reg_dev, dev);
	filp->private_data = dev;

	return 0;
}

/*设备文件释放时调用，空实现*/
static init freg_release(struct inode* inode, struct file* filp){
	return 0;
}


/*读取设备的寄存器val的值*/
static ssize_t freg_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos){
	ssize_t err = 0;
	struct fake_reg_dev* dev = filp->private_data;
	
	/*同步访问*/
	if(down_interruptible(&(dev->sem))){
		return -ERESTARTSYS;
	}

	if(count < sizeof(dev->val)){
		goto out;
	}

	/*将寄存器val的值拷贝到用户提供的缓存区中*/
	if(copy_to_user(buf,&(dev->val),sizeof(dev->val))){
		err = -EFAULT;
		goto out;
	}
	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}


/*写设备的寄存器val的值*/
static ssize_t freg_write(struct file* filp,const char __user *buf,size_t count,loff_t* f_pos){
	struct fake_reg_dev* dev = filp->private_data;
	ssize_t err = 0;

	/*同步访问*/
	if(down_interruptible(&(dev->sem))){
		return -ERESTARTSYS;
	}

	if(count != sizeof(dev->val)){
		goto out;
	}
	if(copy_from_user(&(dev->val),buf,count)){
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);
out:
	up(&(dev->sem));
	return err;
}


/*将寄存器val的值读取到缓存区buf中，内部使用*/
static ssize_t __freg_get_val(struct fake_reg_dev* dev, char* buf){
	int val = 0;

	/*同步访问*/
	if(down_interruptible(&(dev->sem))){
		return -ERESTARTSYS;
	}
	val = dev->val;
	up(&(dev->sem));

	return snprintf(buf,PAGE_SIZE,"%d\n",val);
}

/*把缓存区buf的值写到设备寄存器val中，内部使用*/
static ssize_t __freg_set_val(struct fake_reg_dev* dev,const char* buf, size_t count){
	int val = 0;

	/*将字符串转换成数字*/
	val = simple_strtol(buf,NULL,10);

	/*同步访问*/
	if(down_interruptible(&(dev->sem))){
		return -ERESTARTSYS;
	}
	dev->val = val;
	up(&(dev->sem));

	return count;
}

static ssize_t freg_val_show(struct device* dev,struct device_attribute* attr, char* buf){
	struct fake_reg_dev* hdev = (struct fake_reg_dev*) dev_get_drvdata(dev);

	return __freg_get_val(hdev,buf);
}



static ssize_t freg_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count){
	struct fake_reg_dev* hdev = (struct fake_reg_dev*) dev_get_drvdata(dev);

	return __freg_set_val(hdev,buf,count);
}








