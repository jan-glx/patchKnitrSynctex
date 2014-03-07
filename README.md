patchKintrSynctex
=================

code to patch the Synctex file for use  with knitr (similar to patchDVI for sweave)

USAGE

Save patchKnitrSynctex.R anywhere on your PC. 
Source() in every R Session. 
Use patchKnitrSynctex("pathto/yourmaintexfile.tex") to patch the yourmaintexfile.synctex[.gz] after compilation. 
Enjoy forward and reverse search with your favourite pdf viewer/Latex/Knitr Editor!

For example using StatET with eclipse add a external build configuration (Sweave document Processing) and  use 'Run commands in active R Console' with
require(knitr); 
opts_knit$set(concordance = TRUE); 
texfile <- knit("${resource_loc:${source_file_path}}", encoding="UTF-8")
in the sweave tab and 

syntex <- if (opts_knit$get('concordance'))"-synctex=1" else "-synctex=0";
command=paste("latexmk -pdf", syntex, "-interaction=nonstopmode", shQuote(texfile));
print(paste("Command ",command,"...\n"));
print(shell(command),intern = TRUE);
if (opts_knit$get('concordance')){
	source("C:/Users/Jan/Documents/New folder/patchKnitrSynctex/patchKnitrSynctex.R", echo=FALSE, encoding="UTF-8");
	patchSynctex(texfile); 
}
print(paste0(substr(texfile,1, nchar(texfile)-3), "pdf"))

in the Latex tab.

This R implementation is not optimal. If you are compiling books this may slow down your compile process noticeable.