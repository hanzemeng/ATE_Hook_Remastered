# ATE_Hook_Remastered
A translator for Muv-Luv Alternative Total Eclipse Remastered (ATE).<br>
Those who are looking for a translator for Muv-Luv Alternative Total Eclipse should look into a GitHub repo named ATE_Hook.

## Requirement
This software only works for ATE 1.0.27.

## Installation
Locate the folder where ATE is installed, then copy the following files into that folder:
* ATE_Hook_Remastered.dll
* ATE_Hook_Remastered.exe
* ATE_Hook_Remastered_Color.tsv
* ATE_Hook_Remastered_Config.txt
* ATE_Hook_Remastered_Translation.tsv

**Do not rename any of the files. Do not put any of the files into a folder.**

## Usage
First launch the original game.<br>
After the original game window has a title, launch ATE_Hook_Remastered.exe.<br>
Once the translator is started, the only way to stop it is to close the original game.<br>

## Config
ATE_Hook_Remastered_Config.txt can be modified to change the appearance of the translated text.<br>
To apply new configs, the original game and ATE_Hook_Remastered.exe must both be restarted.<br>
Below describes the configs:
* text_box_length_ratio: A real number in (0, 1). Determines the length of the textbox. A value of 0.5 implies the textbox is half the length of the game window, a value of 1 implies the textbox is the same length of the game window.
* text_box_bottom_offset: An integer in [0, 300). Determines the gap between the bottom of the textbox and the bottom of the game window. A value of 0 implies there is no gap; a value of 50 implies the gap is 50 pixels tall.
* font_name: A string that indicates the font to use when rendering the text. The font must be installed in the operating system.
* font_size: An integer in [0, 64]. Determines the size of the translated text.
* show_text_tag: An integer in {0, 1}. If the value is 1, then the top left corner shows the tag of the current sentence.
* show_original_text: An integer in {0, 1}. If the value is 1, then the game still renders the original text.<br>

**Configs using unexpected values may cause bugs.**

## Translation
**This section is only meant for developers.**<br>
ATE_Hook_Remastered_Translation.tsv contains all of the translations. The file must use utf-8 encoding.<br>
The first column indicates the tag of a sentence, and the second column indicates the translation of that sentence.<br>
Values in the first column should follow this format: $game_t[0-9][0-9][0-9][0-9][0-9]<br>
Values in the second column should not have 0x09 (tab) nor 0x0A (newline).<br>
Values in the second column may use \n to indicate a new line.<br>

## Color
**This section is only meant for developers.**<br>
The color of the text is determined by the cast, and the cast is the characters inside 【】. For example, 篁中尉 is the cast for 【篁中尉】.<br>
One can add/remove/modify cast color in ATE_Hook_Remastered_Color.tsv. The file must use utf-8 encoding.<br>
The first column is the cast. The second column is r; the third column is g; the fourth column is b. rgb must each be within [0, 255].<br>
The first row is the color for sentences with a cast who is not listed in the file.<br>
Note that for sentences without cast (i.e. sentences without 【】), they will always be white.<br>

## License
Those who wish to modify/distribute this software should read the LICENSE first.<br>
