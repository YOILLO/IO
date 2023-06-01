#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/moduleparam.h>
#include <linux/in.h>
#include <net/arp.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <linux/proc_fs.h>

static size_t proc_msg_size = 0;
static char proc_msg[102400];

static char* link = "enp0s3";
module_param(link, charp, 0);

static char* ifname = "vni%d";
static unsigned char data[1500];

static struct net_device_stats stats;

static struct net_device *child = NULL;
struct priv {
    struct net_device *parent;
};

static char check_frame(struct sk_buff *skb, unsigned char data_shift) {
    struct iphdr *ip = (struct iphdr *)skb_network_header(skb);

	if (ntohs(ip->tot_len) - ip->ihl*4 > 70)
            return 0;

    proc_msg_size += sprintf(proc_msg + proc_msg_size, "Packet size: %d ssaddr: %d.%d.%d.%d, daddr: %d.%d.%d.%d\n", ntohs(ip->tot_len) - ip->ihl*4,
                        ntohl(ip->saddr) >> 24, (ntohl(ip->saddr) >> 16) & 0x00FF,
                        (ntohl(ip->saddr) >> 8) & 0x0000FF, (ntohl(ip->saddr)) & 0x000000FF,
                        ntohl(ip->daddr) >> 24, (ntohl(ip->daddr) >> 16) & 0x00FF,
                        (ntohl(ip->daddr) >> 8) & 0x0000FF, (ntohl(ip->daddr)) & 0x000000FF);    
    proc_msg[proc_msg_size] = '\0';
    return 1;
}

static rx_handler_result_t handle_frame(struct sk_buff **pskb) {
    
        if (check_frame(*pskb, 0)) {
            stats.rx_packets++;
            stats.rx_bytes += (*pskb)->len;
        }
        (*pskb)->dev = child;
        return RX_HANDLER_ANOTHER;
} 

static int open(struct net_device *dev) {
    netif_start_queue(dev);
    printk(KERN_INFO "%s: device opened", dev->name);
    return 0; 
} 

static int stop(struct net_device *dev) {
    netif_stop_queue(dev);
    printk(KERN_INFO "%s: device closed", dev->name);
    return 0; 
} 

static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct priv *priv = netdev_priv(dev);

    if (check_frame(skb, 14)) {
        stats.tx_packets++;
        stats.tx_bytes += skb->len;
    }

    if (priv->parent) {
        skb->dev = priv->parent;
        skb->priority = 1;
        dev_queue_xmit(skb);
        return 0;
    }
    return NETDEV_TX_OK;
}

static struct net_device_stats *get_stats(struct net_device *dev) {
    return &stats;
} 

static struct net_device_ops net_device_ops = {
    .ndo_open = open,
    .ndo_stop = stop,
    .ndo_get_stats = get_stats,
    .ndo_start_xmit = start_xmit
};

static void setup(struct net_device *dev) {
    int i;
    ether_setup(dev);
    memset(netdev_priv(dev), 0, sizeof(struct priv));
    dev->netdev_ops = &net_device_ops;

    //fill in the MAC address
    for (i = 0; i < ETH_ALEN; i++)
        dev->dev_addr[i] = (char)i;
} 

static ssize_t fs_read_proc(struct file *f, char __user *buf, size_t len, loff_t *off)
{   
    size_t size = proc_msg_size + 1;

    if (*off > 0 || len < size) 
    {
        return 0;
    }

    char tmp_str[size];

    tmp_str[0] = 0;
    
    strcat(tmp_str, proc_msg);

    if (copy_to_user(buf, tmp_str, size) != 0)
	{
		return -EFAULT;
	}

    *off = len;
    return size;
}

static const struct proc_ops proc_fops = { 
	.proc_read 	= fs_read_proc
};

static struct proc_dir_entry *entry;

int __init vni_init(void) {
    int err = 0;
    struct priv *priv;
    child = alloc_netdev(sizeof(struct priv), ifname, NET_NAME_UNKNOWN, setup);
    if (child == NULL) {
        printk(KERN_ERR "%s: allocate error", THIS_MODULE->name);
        return -ENOMEM;
    }
    priv = netdev_priv(child);
    priv->parent = __dev_get_by_name(&init_net, link); //parent interface
    if (!priv->parent) {
        printk(KERN_ERR "%s: no such net: %s", THIS_MODULE->name, link);
        free_netdev(child);
        return -ENODEV;
    }
    if (priv->parent->type != ARPHRD_ETHER && priv->parent->type != ARPHRD_LOOPBACK) {
        printk(KERN_ERR "%s: illegal net type", THIS_MODULE->name); 
        free_netdev(child);
        return -EINVAL;
    }

    //copy IP, MAC and other information
    memcpy(child->dev_addr, priv->parent->dev_addr, ETH_ALEN);
    memcpy(child->broadcast, priv->parent->broadcast, ETH_ALEN);
    if ((err = dev_alloc_name(child, child->name))) {
        printk(KERN_ERR "%s: allocate name, error %i", THIS_MODULE->name, err);
        free_netdev(child);
        return -EIO;
    }

    register_netdev(child);
    rtnl_lock();
    netdev_rx_handler_register(priv->parent, &handle_frame, NULL);
    rtnl_unlock();
    printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
    printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, child->name);
    printk(KERN_INFO "%s: registered rx handler for %s", THIS_MODULE->name, priv->parent->name);
    
    entry = proc_create("net_test", 0666, NULL, &proc_fops);
    proc_msg[0] = 0;
    return 0; 
}

void __exit vni_exit(void) {
    struct priv *priv = netdev_priv(child);
    if (priv->parent) {
        rtnl_lock();
        netdev_rx_handler_unregister(priv->parent);
        rtnl_unlock();
        printk(KERN_INFO "%s: unregister rx handler for %s", THIS_MODULE->name, priv->parent->name);
    }
    unregister_netdev(child);
    free_netdev(child);
    proc_remove(entry);
    printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name); 
} 

module_init(vni_init);
module_exit(vni_exit);

MODULE_AUTHOR("Author");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Description");
