#pragma once
class MappedInputManager { public: enum class Button { Back, Confirm }; bool isScreenTouchHeld(int&,int&)const{return false;} bool wasScreenTapped(int&,int&)const{return false;} bool wasPressed(Button)const{return false;} };
