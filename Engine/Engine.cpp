#include <Applications/VKSandbox.h>

int main()
{
   {
      CYD::VKSandbox app( 1920, 1080, "GARBAGIO" );
      app.startLoop();
   }

   // To see validation layer errors after destruction
   system( "pause" );
}
