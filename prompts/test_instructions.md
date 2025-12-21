## Build
Go to Ruzino/build and run:
```
ninja.exe
```

But if there are only shader changes, rebuilding is not needed.

## Test Instructions
Run the following command in Ruzino/Binaries/Release:
```
.\.\headless_render.exe -u ..\..\Assets\main_sponza\NewSponza_Main_USD_Zup_003.usdc -j ..\..\Assets\render_nodes_save.json -o profile_sponza.png -w 3840 -h 2160 -s 64
```