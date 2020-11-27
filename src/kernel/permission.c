#include <xbook/account.h>
#include <xbook/memcache.h>
#include <xbook/debug.h>
#include <xbook/fs.h>
#include <xbook/path.h>
#include <string.h>
#include <unistd.h>


permission_database_t *permission_db;

static void permission_data_init(permission_data_t *permdata, uint32_t attr, char *str)
{
    memset(permdata->str, 0, PERMISION_STR_LEN);
    if (str) {
        strcpy(permdata->str, str);
        permdata->str[strlen(str)] = '\0';
    }
    permdata->attr = attr;
}

void permission_database_dump()
{
    pr_dbg("Permission database length:%d\n", permission_db->length);
    int i;
    for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        permission_data_t *data = &permission_db->datasets[i];
        if (data->attr) {
            pr_dbg("%d: attr:%x str:%s\n", i, data->attr, data->str);
        }
    }
}

int permission_database_init()
{
    permission_db = mem_alloc(sizeof(permission_database_t));
    if (!permission_db) {
        panic("permission database alloc failed!\n");
    }
    mutexlock_init(&permission_db->lock);
    permission_db->length = 0;
    int i;
    for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        permission_data_init(&permission_db->datasets[i], 0, NULL);
    }
    permission_database_load();
    return 0;
}

int permission_database_freesolt()
{
    int i;
    for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        if (!permission_db->datasets[i].attr)
            break;
    }
    if (i >= PERMISION_DATABASE_LEN)
        return -1;
    return i;
}

int permission_database_insert(uint32_t attr, char *str)
{
    mutex_lock(&permission_db->lock);
    if (permission_db->length >= PERMISION_DATABASE_LEN) {
        mutex_unlock(&permission_db->lock);
        return -1;
    }
    int solt = permission_database_freesolt();
    if (solt < 0) {
        mutex_unlock(&permission_db->lock);
        return -1;
    }
    permission_data_init(&permission_db->datasets[solt], attr, str);
    permission_db->length++;
    mutex_unlock(&permission_db->lock);
    return solt;
}

int permission_database_delete(uint32_t index)
{
    mutex_lock(&permission_db->lock);
    if (index >= PERMISION_DATABASE_LEN) {
        mutex_unlock(&permission_db->lock);
        return -1;
    }
    permission_data_init(&permission_db->datasets[index], 0, NULL);
    permission_db->length--;
    mutex_unlock(&permission_db->lock);
    return 0;
}

int permission_database_delete_by_data(char *str)
{
    mutex_lock(&permission_db->lock);
    
    int i; for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        if (!strcmp(permission_db->datasets[i].str, str)) {
            permission_data_init(&permission_db->datasets[i], 0, NULL);
            permission_db->length--;
            mutex_unlock(&permission_db->lock);        
            return 0;
        }
    }
    mutex_unlock(&permission_db->lock);
    return -1;
}

permission_data_t *permission_database_select(char *str)
{
    mutex_lock(&permission_db->lock);
    int i;
    for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        permission_data_t *data = &permission_db->datasets[i];
        if (data->attr && !strcmp(data->str, str)) {
            mutex_unlock(&permission_db->lock);    
            return data;
        }
    }
    mutex_unlock(&permission_db->lock);
    return NULL;
}

permission_data_t *permission_database_select_by_index(uint32_t index)
{
    if (index >= PERMISION_DATABASE_LEN) {
        return NULL;   
    }
    return &permission_db->datasets[index];
}

void permission_database_foreach(void (*callback)(void *, void *) , void *arg)
{
    int i;
    for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        if (permission_db->datasets[i].attr)
            callback(arg, &permission_db->datasets[i]);
    }
}

static void permission_data_to_buf(permission_data_t *data, char *buf)
{
    char s[2] = {0};
    if (data->attr & PERMISION_ATTR_DEVICE) {
        s[0] = '0';
    } else if (data->attr & PERMISION_ATTR_FILE) {
        s[0] = '1';
    } else if (data->attr & PERMISION_ATTR_FIFO) {
        s[0] = '2';
    } else{
        s[0] = '-';
    }
    strcat(buf, s);
    if (data->attr & PERMISION_ATTR_READ) {
        s[0] = 'R';
    } else{
        s[0] = '-';
    }
    strcat(buf, s);
    if (data->attr & PERMISION_ATTR_WRITE) {
        s[0] = 'W';
    } else{
        s[0] = '-';
    }
    strcat(buf, s);
    if (data->attr & PERMISION_ATTR_EXEC) {
        s[0] = 'X';
    } else{
        s[0] = '-';
    }
    strcat(buf, s);
    if (data->attr & PERMISION_ATTR_HOME) {
        s[0] = 'H';
    } else{
        s[0] = '-';
    }
    strcat(buf, s);
    strcat(buf, ",");
    strcat(buf, data->str);
    strcat(buf, "\n");
}

int permission_database_sync()
{
    mutex_lock(&permission_db->lock);

    char buf[32] = {0};
    strcat(buf, ACCOUNT_DIR_PATH);
    strcat(buf, "/");
    strcat(buf, PERMISSION_FILE_NAME);
    int fd = kfile_open(buf, O_RDWR | O_TRUNC);
    if (fd < 0) {
        pr_err("open permission file %s failed!\n", buf);
        mutex_unlock(&permission_db->lock);
        return -1;
    }
    int i; for (i = 0; i < PERMISION_DATABASE_LEN; i++) {
        permission_data_t *data = &permission_db->datasets[i];
        if ((data->attr)) {
            /* attr,str\n */
            char permission_buf[PERMISION_STR_LEN + 12] = {0};
            permission_data_to_buf(data, permission_buf);
            // pr_dbg("permission buf:%s", permission_buf);
            kfile_write(fd, permission_buf, strlen(permission_buf));
        }
    }
    kfile_close(fd);
    mutex_unlock(&permission_db->lock);
    return 0;
}

/* 自动添加到权限库中 */
int permission_auto_insert()
{
    char *str;
    str = "disk0"; permission_database_insert(PERMISION_ATTR_DEVICE | PERMISION_ATTR_RDWR, str);
    str = "disk1"; permission_database_insert(PERMISION_ATTR_DEVICE | PERMISION_ATTR_RDWR, str);
    str = ACCOUNT_DIR_PATH; permission_database_insert(PERMISION_ATTR_FILE | PERMISION_ATTR_RDWR, str);
    str = "/sbin/init"; permission_database_insert(PERMISION_ATTR_FILE | PERMISION_ATTR_RDWR, str);
    /* 同步到数据库中去 */
    if (permission_database_sync() < 0) {
        return -1;
    }
    return 0;
}

int permission_scan_line(char *str)
{
    // 权限,内容
    char *next = strchr(str, ',');
    if (!next)
        return -1;
    *next = '\0';
    next++;
    if (!next)
        return -1;
    /* 解析权限 */
    uint32_t attr = 0;
    char *p = str;
    if (*p == '0') {
        attr |= PERMISION_ATTR_DEVICE;
    } else if (*p == '1') {
        attr |= PERMISION_ATTR_FILE;
    } else if (*p == '2') {
        attr |= PERMISION_ATTR_FIFO;
    }
    p++;
    if (*p == 'R') {
        attr |= PERMISION_ATTR_READ;
    }
    p++;
    if (*p == 'W') {
        attr |= PERMISION_ATTR_WRITE;
    }
    p++;
    if (*p == 'X') {
        attr |= PERMISION_ATTR_EXEC;
    }
    p++;
    if (*p == 'H') {
        attr |= PERMISION_ATTR_HOME;
    }
    /* 解析数据，去掉换行符号，windows时\r\n,linux是\n,mac是\r。 */
    p = strchr(next, '\n');
    if (p)
        *p = '\0';
    p = strchr(next, '\r');
    if (p)
        *p = '\0';
    return (permission_database_insert(attr, next) >= 0) ? 0: -1;
}

/* 扫描文件内容，并添加到权限库中 */
static int permission_scan_insert(char *filename)
{
    int fd = kfile_open(filename, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    int fsize = kfile_lseek(fd, 0, SEEK_END);
    if (fsize <= 0) {
        kfile_close(fd);
        return -1;
    }
    char *buf = mem_alloc(fsize);
    if (!buf) {
        kfile_close(fd);
        return -1;
    }
    memset(buf, 0, fsize);
    kfile_lseek(fd, 0, SEEK_SET);
    if (kfile_read(fd, buf, fsize) != fsize) {
        mem_free(buf);
        kfile_close(fd);
        return -1;
    }
    kfile_close(fd);
    char *str = buf;
    char *p = str;
    char *next_line = p;
    while (*p && next_line < buf + fsize) {
        while (*p != '\n' && *p) {
            p++;
        }
        next_line = p + 1;
        *p = 0;
        if (permission_scan_line(str) < 0) {
            mem_free(buf);
            return -1;
        }
        str = next_line;
        p = str;
    }
    mem_free(buf);
    return 0;
}

int permission_database_load()
{
    char buf[32] = {0};
    strcat(buf, ACCOUNT_DIR_PATH);
    strcat(buf, "/");
    strcat(buf, PERMISSION_FILE_NAME);
    /* 如果文件不存在，则需要创建文件，并创建根账户 */
    int file_not_exist = 0;
    int fd = -1;
    if (kfile_access(buf, F_OK) < 0) {
        fd = kfile_open(buf, O_CREAT | O_RDWR);
        if (fd < 0) {
            pr_err("create permision file %s failed!\n", buf);
            return -1;
        }
        kfile_close(fd);
        file_not_exist = 1;
    }
    if (file_not_exist) {   
        if (permission_auto_insert() < 0) {
            pr_err("permision sync file %s failed!\n", buf);
            return -1;
        }
    } else {
        /* 存在则直接从文件中解析 */
        if (permission_scan_insert(buf) < 0) {
            pr_err("permision scan file %s failed!\n", buf);
            return -1;
        }
    }
    return 0;
}
