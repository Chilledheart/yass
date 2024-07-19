# Update Zlib

```
rsync -aPvx ~/chromium/src/third_party/zlib .
rm -f zlib/BUILD.gn zlib/OWNERS zlib/README.chromium
```

# Update modp_64
```
rsync -aPvx ~/chromium/src/third_party/modp_b64 .
rm -f modp_b64/BUILD.gn modp_b64/DEPS modp_b64/OWNERS modp_b64/DIR_METADATA modp_b64/README.chromium
```
