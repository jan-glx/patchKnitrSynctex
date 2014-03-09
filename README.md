patchKintrSynctex
=================

code to patch the Synctex file for use  with knitr (similar to `patchDVI` for sweave)

Usage
------

Save `patchKnitrSynctex.R` anywhere on your PC. 
`source()` in every R Session. 
Use `patchKnitrSynctex("pathto/yourmaintexfile.tex")` to patch the yourmaintexfile.synctex[.gz] after compilation. 
Enjoy forward and reverse search with your favourite pdf viewer/Latex/Knitr Editor!

Example
------

For example using StatET with eclipse add a external build configuration (Sweave document Processing) and  use 'Run commands in active R Console' with...
```r
require(knitr); 
opts_knit$set(concordance = TRUE); 
texfile <- knit("${resource_loc:${source_file_path}}", encoding="UTF-8")
```
...in the sweave tab and... 
```r
syntex <- if (opts_knit$get('concordance'))"-synctex=1" else "-synctex=0";
command=paste("latexmk -pdf", syntex, "-interaction=nonstopmode", shQuote(texfile));
print(paste("Command ",command,"...\n"));
print(shell(command),intern = TRUE);
if (opts_knit$get('concordance')){
	source("C:/Users/Jan/Documents/New folder/patchKnitrSynctex/patchKnitrSynctex.R", echo=FALSE, encoding="UTF-8");
	patchSynctex(texfile); 
}
print(paste0(substr(texfile,1, nchar(texfile)-3), "pdf"))
```
...in the Latex tab.

Remarks
----
This will break the Byte offsets in the sy
This R implementation is not optimal. If you are compiling books this may slow down your compile process noticeable.

Acknowledgement
-------------
Code was inspired by [patchDVI](http://cran.r-project.org/web/packages/patchDVI/index.html), [RStudio](https://github.com/rstudio/rstudio) and [Synctex](http://itexmac.sourceforge.net/SyncTeX.html) code.
I am not affiliated with [StatET](https://github.com/walware/statet), [Eclipse](http://git.eclipse.org/c/), [Knitr](https://github.com/yihui/knitr), [RStudio](https://github.com/rstudio/rstudio), [Synctex](http://itexmac.sourceforge.net/SyncTeX.html) or [patchDVI](http://cran.r-project.org/web/packages/patchDVI/index.html). They did all the work !
