AvNav OchartsNG
===============
This repo contains a new implementation for the [o-charts](https://o-charts.org/?lng=en) support in [AvNav](https://www.wellenvogel.net/software/avnav/docs/hints/ocharts.html?lang=en).

It is based on the [avnav-ocharts-provider](https://github.com/wellenvogel/avnav-ocharts-provider) and the [OpenCPN](https://github.com/OpenCPN/OpenCPN) and [o-charts_pi](https://github.com/bdbcat/o-charts_pi) code. Especially it uses the display mechanisms and chart encryption handling from [o-charts_pi](https://github.com/bdbcat/o-charts_pi).

The functionality is very similar to the original avnav-ocharts-provider. But this new plugin is also available for android.

Currently the project is in a beta status. 
Documentation is still missing at many points.

License
-------
The license for the project is mixed.

The [gui](gui/) part and the [avnav plugin](avnav-plugin/) are [MIT](LicenseMIT.md) licensed.<br>
The [provider](provider/) part is licensed with [GPLv2](LicenseGPL.md) - except for:
  * [preload](provider/preload/) - licensed [MIT](LicenseMIT.md)
  * [tokenHandler](provider/tokenHandler/) - licensed by a [restricted license](LicenseP.md)


Status
------
  * Nearly complete support for oeSENC charts.
  * no support for oeRNC charts
  * still some optimizations and caches missing
