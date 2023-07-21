struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  // delete lab8-2
  //struct buf *prev; // LRU cache list
  //struct buf *next;
  uchar data[BSIZE];

  // add the following 2 lines
    uint tstamp;  // 本缓存块的最近访问时间戳
    uint bktord;  // 本缓存块在其哈希桶数组中的顺序

};

