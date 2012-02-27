rem @echo off
sed -f %1.sed < template_inf | sed -f %2.sed > %1.inf
copy template_cat %1.cat
copy template_cat %1_x64.cat