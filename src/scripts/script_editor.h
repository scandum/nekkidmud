#ifndef SCRIPT_EDITOR_H
#define SCRIPT_EDITOR_H
//*****************************************************************************
//
// script_editor.h
//
// this is an extention of the basic script editor (/src/editor/editor.h) to
// be tailored for script editing. Provides syntax coloring and line number
// display, and auto indenting.
//
//*****************************************************************************

extern EDITOR *script_editor;

//
// prepare the script editor for use
//
void init_script_editor();

#endif // SCRIPT_EDITOR_H
