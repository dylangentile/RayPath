#include "render.h"



int 
main(void)
{
	Renderer renderer("Sphere", 1280, 720, false);
	renderer.init();



	renderer.quit();
	return 0;
}



