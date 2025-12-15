#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/types.h>

#define I2C_BUS_AVAILABLE           (1)
#define SLAVE_DEVICE_NAME           ("ETX_OLED")
#define SSD130_SLAVE_ADDR           (0x3C)

#define ETX_IOCTL_BASE              'W'
#define ETX_IOCTL_SET_BRIGHTNESS    _IOW(ETX_IOCTL_BASE, 1, int)
#define ETX_IOCTL_SET_ROTATION      _IOW(ETX_IOCTL_BASE, 2, int)
#define ETX_IOCTL_CLEAR_DISPLAY     _IO(ETX_IOCTL_BASE, 3)
#define ETX_IOCTL_SET_DISPLAY_MODE  _IOW(ETX_IOCTL_BASE, 4, int)
#define ETX_IOCTL_SET_PAGE          _IOW(ETX_IOCTL_BASE, 5, int)
#define ETX_IOCTL_SET_COL_LOW       _IOW(ETX_IOCTL_BASE, 6, int)
#define ETX_IOCTL_SET_COL_HIGH      _IOW(ETX_IOCTL_BASE, 7, int)

#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define BUFFER_SIZE     (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)
#define PAGE_NUM        8
#define COL_NUM         128

static unsigned char display_buffer[PAGE_NUM][COL_NUM];
static unsigned char display_buffer_1d[PAGE_NUM * COL_NUM];

static int oled_brightness      = 128;
static int oled_rotation        = 0;
static int oled_display_mode    = 0;

static struct i2c_adapter   *etx_i2c_adapter     = NULL;
static struct i2c_client    *etx_i2c_client_oled = NULL;

static dev_t dev = 0;
static struct class *st_dev_class;
static struct cdev file_ops_cdev;

static int I2C_Write(unsigned char *buf, unsigned int len)
{
    /*
    ** Sending Start condition, Slave address withR/W bit
    ** ACK/NACK and Stop condition will be handled internally.
    */
   pr_info("Send data!\n");
   int ret = i2c_master_send(etx_i2c_client_oled, buf, len);
   return ret;
}
/* This function reads one byte of the data from the I2C client*/
static int I2C_Read(unsigned char *out_buf, unsigned int len)
{
    /*
    ** Sending Start condition, Slave address with R/W but,
    ** ACK/NACK and Stop conditions will be handled internally.
    */
   int ret = i2c_master_recv(etx_i2c_client_oled, out_buf, len);
   return ret;
}

static void SSD1306_Write(bool is_cmd, unsigned char *data, unsigned int len)
{
    unsigned char buf[len + 1];  // +1 for command/data prefix
    int ret;

    if (is_cmd)
        buf[0] = 0x00;  // Command prefix
    else
        buf[0] = 0x40;  // Data prefix

    memcpy(&buf[1], data, len);  // Copy data after prefix

    pr_info("Start write buffer!\n");
    ret = I2C_Write(buf, len + 1);  // Write prefix + data
}

/* This function sends the commands that need to used to initialize OLED */
static int SSD1306_DisplayInit(void)
{
    msleep(100);
    unsigned char cmd;

    /* Commands to initialize the SSD_1306 OLED Display*/
    cmd = 0xAE; SSD1306_Write(true, &cmd, 1); // Entire Display OFF
    cmd = 0xD5; SSD1306_Write(true, &cmd, 1); // Set Display Clock Divide Ratio and Oscillator Frequency
    cmd = 0x80; SSD1306_Write(true, &cmd, 1); // Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
    cmd = 0xA8; SSD1306_Write(true, &cmd, 1); // Set Multiplex Ratio
    cmd = 0x3F; SSD1306_Write(true, &cmd, 1); // 64 COM lines
    cmd = 0xD3; SSD1306_Write(true, &cmd, 1); // Set display offset
    cmd = 0x00; SSD1306_Write(true, &cmd, 1); // 0 offset
    cmd = 0x40; SSD1306_Write(true, &cmd, 1); // Set first line as the start line of the display
    cmd = 0x8D; SSD1306_Write(true, &cmd, 1); // Charge pump
    cmd = 0x14; SSD1306_Write(true, &cmd, 1); // Enable charge dump during display on
    cmd = 0x20; SSD1306_Write(true, &cmd, 1); // Set memory addressing mode
    cmd = 0x00; SSD1306_Write(true, &cmd, 1); // Horizontal addressing mode
    cmd = 0xA1; SSD1306_Write(true, &cmd, 1); // Set segment remap with column address 127 mapped to segment 0
    cmd = 0xC8; SSD1306_Write(true, &cmd, 1); // Set com output scan direction, scan from com63 to com 0
    cmd = 0xDA; SSD1306_Write(true, &cmd, 1); // Set com pins hardware configuration
    cmd = 0x12; SSD1306_Write(true, &cmd, 1); // Alternative com pin configuration, disable com left/right remap
    cmd = 0x81; SSD1306_Write(true, &cmd, 1); // Set contrast control
    cmd = 0x80; SSD1306_Write(true, &cmd, 1); // Set Contrast to 128
    cmd = 0xD9; SSD1306_Write(true, &cmd, 1); // Set pre-charge period
    cmd = 0xF1; SSD1306_Write(true, &cmd, 1); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
    cmd = 0xDB; SSD1306_Write(true, &cmd, 1); // Set Vcomh deselect level
    cmd = 0x20; SSD1306_Write(true, &cmd, 1); // Vcomh deselect level ~ 0.77 Vcc
    cmd = 0xA4; SSD1306_Write(true, &cmd, 1); // Entire display ON, resume to RAM content display
    cmd = 0xA6; SSD1306_Write(true, &cmd, 1); // Set Display in Normal Mode, 1 = ON, 0 = OFF
    cmd = 0x2E; SSD1306_Write(true, &cmd, 1); // Deactivate scroll
    cmd = 0xAF; SSD1306_Write(true, &cmd, 1); // Display ON in normal mode

    return 0;
}

/* This function fills the complete OLED with this data byte */
static void SSD1306_Fill(unsigned char data)
{
    unsigned int total = 128 * 8; // 8 pages x 128 segments
    unsigned char buf[total];

    // Fill the buffer with the data byte
    for (unsigned int i = 0; i < total; i++)
    {
        buf[i] = data;
    }

    // Before filling, set the page and column addresses properly
    unsigned char cmd;

    for (int page = 0; page < 8; page++)
    {
        cmd = 0xB0 + page; // Set page address
        SSD1306_Write(true, &cmd, 1);

        cmd = 0x00; // Set lower column start address to 0
        SSD1306_Write(true, &cmd, 1);

        cmd = 0x10; // Set higher column start address to 0
        SSD1306_Write(true, &cmd, 1);

        // Send 128 bytes of data for the current page
        SSD1306_Write(false, &buf[page * 128], 128);
    }
}

static void oled_clear_display(void)
{
    memset(display_buffer, 0x00, PAGE_NUM * COL_NUM * sizeof(uint8_t));
    unsigned char data = 0x00;
    SSD1306_Fill(data);
}

static void oled_set_brightness(int level)
{
    unsigned char cmd;
    if(level < 0) level = 0;
    else if(level > 255) level = 255;

    oled_brightness = level;
    cmd = 0x81; SSD1306_Write(true, &cmd, 1); // Constast control command
    cmd = oled_brightness; SSD1306_Write(true, &cmd, 1);
}

static void oled_set_rotation(int rotation)
{
    unsigned char cmd;
    oled_rotation = rotation;

    switch(rotation)
    {
        case 0:
            cmd = 0xA1; SSD1306_Write(true, &cmd, 1); // segment remap column address 127 to 0
            cmd = 0xC8; SSD1306_Write(true, &cmd, 1); // COM output scan direction remapped
            break;
        case 1:
            cmd = 0xA0; SSD1306_Write(true, &cmd, 1);
            cmd = 0xC0; SSD1306_Write(true, &cmd, 1);
            break;
        default:
            break;
    }
}

static void oled_set_display_mode(int mode)
{
    unsigned char cmd;
    oled_display_mode = mode;
    switch(mode)
    {
        case 0:
            cmd = 0xA6; SSD1306_Write(true, &cmd, 1);
            break;
        case 1:
            cmd = 0xA7; SSD1306_Write(true, &cmd, 1);
            break;
        default:
            break;
    }
}

static long etx_oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned char cmd_in;
    int val = 0;

    switch(cmd)
    {
        case ETX_IOCTL_SET_BRIGHTNESS:
            if(copy_from_user(&val, (int __user *)arg, sizeof(val)))
                return -EFAULT;
            oled_set_brightness(val);
            break;
        case ETX_IOCTL_SET_ROTATION:
            if(copy_from_user(&val, (int __user *)arg, sizeof(val)))
                return -EFAULT;
            oled_set_rotation(val);
            break;
        case ETX_IOCTL_CLEAR_DISPLAY:
            oled_clear_display();
            break;
        case ETX_IOCTL_SET_DISPLAY_MODE:
            if(copy_from_user(&val, (int __user *)arg, sizeof(val)))
                return -EFAULT;
            oled_set_display_mode(val);
            break;
        case ETX_IOCTL_SET_PAGE:
            if (copy_from_user(&val, (uint8_t __user *)arg, sizeof(val)))
                return -EFAULT;
            cmd_in = 0xB0 + val; SSD1306_Write(true, &cmd_in, 1);
            break;
        case ETX_IOCTL_SET_COL_LOW:
            if (copy_from_user(&val, (uint8_t __user *)arg, sizeof(val)))
                return -EFAULT;
            cmd_in = val; SSD1306_Write(true, &cmd_in, 1);  // ở đây giá trị trỏ vào 0x00
            break;
        case ETX_IOCTL_SET_COL_HIGH:
            if (copy_from_user(&val, (uint8_t __user *)arg, sizeof(val)))
                return -EFAULT;
            cmd_in = val; SSD1306_Write(true, &cmd_in, 1);  // ở đây giá trị trỏ vào 0x10
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

// static ssize_t etx_oled_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
// {
//     ssise_t ret;
//     size_t bytes_to_read;

//     if(*ppos >= BUFFER_SIZE)
//         return 0; //EOF
    
//     bytes_to_read = min(count, (size_t)(BUFFER_SIZE - *ppos));

//     if(copy_to_user(buf, display_buffer + *ppos, bytes_to_read))
//         return -EFAULT;

//     *ppos += bytes_to_read;
//     ret = bytes_to_read;

//     printk(KERN_INFO  "Read function!\n");

//     return 0;
// }

void print_display_buffer(uint8_t *buffer, size_t size)
{
    size_t i;
    for (i = 0; i < size; i++) {
        printk(KERN_INFO "Buf[%zu] = 0x%02X\n", i, buffer[i]);
    }
}

// static ssize_t etx_oled_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
// {
//     size_t total_bytes = PAGE_NUM * COL_NUM;
//     size_t bytes_to_write = (count > total_bytes) ? total_bytes : count;
//     uint16_t count_print = 0;
//     unsigned char cmd;
//     printk(KERN_INFO "Number of bytes to write: %d \n", bytes_to_write);

//     // if(copy_from_user(&display_buffer[0][0], buf, bytes_to_write))
//     if(copy_from_user(&display_buffer_1d, buf, bytes_to_write))
//         return -EFAULT;
    
//     // print_display_buffer(display_buffer_1d, 1024);
//     pr_info("display_buffer[0] = %d\n", display_buffer_1d[0]);
//     pr_info("display_buffer[1] = %d\n", display_buffer_1d[1]);
//     pr_info("display_buffer[128] = %d\n", display_buffer_1d[128]);
//     pr_info("display_buffer[129] = %d\n", display_buffer_1d[129]);

//     for(int page = 0; page < PAGE_NUM; page++)
//     {   
//         cmd = 0xB0+page; SSD1306_Write(true, &cmd, 1);              // Set page start address
//         cmd = 0x00; SSD1306_Write(true, &cmd, 1);              // Set lower column start address
//         cmd = 0x10; SSD1306_Write(true, &cmd, 1);              // Set higher column start address
//         // send data column in page
//         // for(int col = 0; col < COL_NUM; col++)
//         // {
//         //     SSD1306_Write(false, display_buffer_1d[page * COL_NUM + col]);
//         //     // msleep(500);
//         // }
//         unsigned char buf_page[1+COL_NUM];
//         // buf[0] = 0x40; // Data prefix
//         memcpy(&buf_page[1], (unsigned char *) &display_buffer_1d[page * COL_NUM], COL_NUM);
//         SSD1306_Write(false, &buf_page[0], COL_NUM);
//         // I2C_Write(buf, 1+COL_NUM); // Send 129 bytes in 1 go
//     }

//     return bytes_to_write;
// }


static ssize_t etx_oled_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    size_t total_bytes = PAGE_NUM * COL_NUM; // PAGE_NUM = 8, COL_NUM = 128
    size_t bytes_to_write = (count > total_bytes) ? total_bytes : count;
    unsigned char cmd;
    printk(KERN_INFO "Number of bytes to write: %zu\n", bytes_to_write);

    // Copy data từ user space vào kernel buffer
    if (copy_from_user(display_buffer_1d, buf, bytes_to_write))
        return -EFAULT;

    // Debug vài byte
    pr_info("display_buffer[0] = %d\n", display_buffer_1d[0]);
    pr_info("display_buffer[1] = %d\n", display_buffer_1d[1]);
    pr_info("display_buffer[128] = %d\n", display_buffer_1d[128]);
    pr_info("display_buffer[129] = %d\n", display_buffer_1d[129]);

    // 1. Set column range: 0 -> 127
    unsigned char col_cmds[] = {0x21, 0x00, 0x7F};
    SSD1306_Write(true, col_cmds, sizeof(col_cmds));

    // 2. Set page range: 0 -> 7
    unsigned char page_cmds[] = {0x22, 0x00, 0x07};
    SSD1306_Write(true, page_cmds, sizeof(page_cmds));

    // 3. Burst full frame: prefix + 1024 bytes
    unsigned char buf_frame[1 + total_bytes];
    buf_frame[0] = 0x40; // Data prefix
    memcpy(&buf_frame[1], display_buffer_1d, total_bytes);

    // Gửi toàn bộ frame trong một lần
    SSD1306_Write(false, buf_frame, sizeof(buf_frame));

    return bytes_to_write;
}

static const struct file_operations etx_oled_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = etx_oled_ioctl,
    // .read           = etx_oled_read,
    .write          = etx_oled_write,
};

/* This function getting called when the slave has been found */
static int etx_oled_probe(struct i2c_client *client)
{
    SSD1306_DisplayInit();

    // Fill the OLED with this data
    SSD1306_Fill(0xFF); // 0x00 is white, 0xFF is full

    pr_info("OLED Probed!!!\n");

    return 0;
}

/* This function getting called when the slave has been remove */
static void etx_oled_remove(struct i2c_client *client)
{
    // Fill the OLED with this data
    SSD1306_Fill(0x00);

    pr_info("OLED Removed!!!\n");
}

/* Structure that has slave device id*/
static const struct i2c_device_id etx_oled_id[] = {
    { SLAVE_DEVICE_NAME, 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);

/* I2C driver structure that has to be added to Linux */
static struct i2c_driver etx_oled_driver = {
    .driver = {
        .name   = SLAVE_DEVICE_NAME,
        .owner  = THIS_MODULE,
    },
    .probe      = etx_oled_probe,
    .remove     = etx_oled_remove,
    .id_table   = etx_oled_id,
};

/* I2C Board Info Structure */
static struct i2c_board_info oled_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD130_SLAVE_ADDR)
};

/* Module Init function */
static int __init etx_driver_init(void)
{
    int ret = 0;
    etx_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

    if( etx_i2c_adapter != NULL )
    {
        etx_i2c_client_oled = i2c_new_client_device(etx_i2c_adapter, &oled_i2c_board_info);

        if( etx_i2c_client_oled != NULL )
        {
            i2c_add_driver(&etx_oled_driver);
            ret = 0;
        }
        else
        {
            pr_err("Failed to register the I2C OLED client device\n");
            ret = -1;
            return ret;
        }

        i2c_put_adapter(etx_i2c_adapter);
    }
    else
    {
        pr_err("Failed to get I2C adapter\n");
        ret = -1;
        return ret;
    }

    if(alloc_chrdev_region(&dev, 0, 1, "oled_device_chrdev") < 0)
    {
        printk(KERN_INFO "Dynamic Allocate failed! \n");
        return -1;
    }
    printk("Dynamic allocate successfully with Major = %d, Minor = %d \n", MAJOR(dev), MINOR(dev));

    cdev_init(&file_ops_cdev, &etx_oled_fops);
    // Adding character device to the system
    if((cdev_add(&file_ops_cdev, dev, 1)) < 0)
    {
        pr_err("Cannot add the device to the system! \n");
        goto err_class;
    }
    
    // Newer kernels (6.4 and later): class_create now takes only 1 argument - the class name string.
    st_dev_class = class_create("etx_oled_class");
    if(IS_ERR(st_dev_class))
    {
        pr_err("Cannot create struct class for this device! \n");
        goto err_class;
    }
    printk(KERN_INFO "Create struct class successfully for this device");

    if(IS_ERR(device_create(st_dev_class, NULL, dev, NULL, "etx_oled_device")))
    {
        pr_err("Cannot create device file! \n");
        goto err_device;
    }
    printk(KERN_INFO "Create device file successfuly! \n");

    pr_info("Driver Added!!!\n");
    return ret;

err_device:
    class_destroy(st_dev_class);

err_class:
    unregister_chrdev_region(dev, 1);
    return -1;
}

/* Module Exit function */
static void __exit etx_driver_exit(void)
{
    i2c_unregister_device(etx_i2c_client_oled);
    i2c_del_driver(&etx_oled_driver);
    device_destroy(st_dev_class, dev);
    class_destroy(st_dev_class);
    cdev_del(&file_ops_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Driver Removed!!!\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX");
MODULE_DESCRIPTION("Simple I2C driver explanation (SSD_130 OLED)");
MODULE_VERSION("1.0");
