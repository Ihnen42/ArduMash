/*
  Morse.h - Library for flashing Morse code.
  Created by David A. Mellis, November 2, 2007.
  Released into the public domain.
*/
#ifndef Brew_h
#define Brew_h

#include "Arduino.h"

typedef struct recipeStep {
  int   recipe;
  int   recipeStep;
  float targetTemp;
  unsigned long duration;
  bool  autoContinue;
  unsigned long runTime;
  int   stepStatus;
};



class Brew
{
  public:
    Brew();
    void loadDefaulRecipe();
    void setRecipe();
    
  private:
     recipeStep _recipe[10];
};

#endif
