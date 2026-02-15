# Arch Linux liquid-dsp 1.7.0 FEC 可用性

> 编译时未链接 libfec，卷积码和 RS 码不可用

## 可用（liquid-dsp 内置实现）

| 方案 | 配置名 | 编码后长度(64B) | 说明 |
|------|--------|-----------------|------|
| none | `none` | 64 | 无纠错 |
| rep3 | `rep3` | 192 | 3 倍重复码 |
| rep5 | `rep5` | 320 | 5 倍重复码 |
| Hamming(7,4) | `h74` | 112 | |
| Hamming(8,4) | `h84` | 128 | |
| Hamming(12,8) | `h128` | 96 | |
| Golay(24,12) | `g2412` | 129 | |
| SEC-DED(22,16) | `secded2216` | 96 | |
| SEC-DED(39,32) | `secded3932` | 80 | |
| SEC-DED(72,64) | `secded7264` | 72 | |

## 不可用（需要外部 libfec）

| 方案 | 配置名 | 错误信息 |
|------|--------|----------|
| Convolutional V27 | `conv27` | convolutional codes unavailable (install libfec) |
| Convolutional V29 | `conv29` | 同上 |
| Convolutional V39 | `conv39` | 同上 |
| Convolutional V615 | `conv615` | 同上 |
| Conv V27 p2/3 | `conv27p23` | 同上 |
| Conv V27 p3/4 | `conv27p34` | 同上 |
| Conv V27 p4/5 | `conv27p45` | 同上 |
| Conv V27 p5/6 | `conv27p56` | 同上 |
| Conv V27 p6/7 | `conv27p67` | 同上 |
| Conv V27 p7/8 | `conv27p78` | 同上 |
| Conv V29 p2/3 | `conv29p23` | 同上 |
| Conv V29 p3/4 | `conv29p34` | 同上 |
| Conv V29 p4/5 | `conv29p45` | 同上 |
| Conv V29 p5/6 | `conv29p56` | 同上 |
| Conv V29 p6/7 | `conv29p67` | 同上 |
| Conv V29 p7/8 | `conv29p78` | 同上 |
| Reed-Solomon M8 | `rs8` | Reed-Solomon codes unavailable (install libfec) |
