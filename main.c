

// Dołączenie wszelkich potrzebnych plików nagłówkowych
#include "usbstk5515.h"
#include "usbstk5515_led.h"
#include "aic3204.h"
#include "PLL.h"
#include "bargraph.h"
#include "oled.h"
#include "pushbuttons.h"
#include "dsplib.h"

//Krok fazy dla sygnału piłokształtnego (100Hz)
#define KROK_FAZY_PILO 136
// Częstotliwość próbkowania (48 kHz)
#define CZESTOTL_PROBKOWANIA 48000L
// Wzmocnienie wejścia w dB: 0 dla wejścia liniowego, 30 dla mikrofonowego
#define WZMOCNIENIE_WEJSCIA_dB 30

#define X 2048
#define N 17
#define fc 1760


const short IIR_wspolczynniki[] =
{
 189, 378, 189, -30541, 14986,
 189, 378, 189, -28310, 12694,
 189, 378, 189, -26812, 11156,
 189, 378, 189, -26065, 10387
};

int akumulator_fazy;

int bufor[X];
int index = 0;
int i = 0;
int index_pomocniczy= 0;
short bufor_roboczy[17];

int realis = 0;
int imaginalis = 0;

void rand16init();

// Główna procedura programu

void main(void) {

    // Inicjalizacja układu DSP
    USBSTK5515_init();          // BSL
    pll_frequency_setup(100);   // PLL 100 MHz
    aic3204_hardware_init();    // I2C
    aic3204_init();             // kodek dźwięku AIC3204
    USBSTK5515_ULED_init();     // diody LED
    SAR_init_pushbuttons();     // przyciski
    oled_init();                // wyświelacz LED 2x19 znaków

    // ustawienie częstotliwości próbkowania i wzmocnienia wejścia
    set_sampling_frequency_and_gain(CZESTOTL_PROBKOWANIA, WZMOCNIENIE_WEJSCIA_dB);

    // wypisanie komunikatu na wyświetlaczu
    // 2 linijki po 19 znaków, tylko wielkie angielskie litery
    oled_display_message("PROJEKT ZPS        ", "                   ");

    // 'tryb_pracy' oznacza tryb pracy wybrany przyciskami
    unsigned int tryb_pracy = 0;
    unsigned int poprzedni_tryb_pracy = 99;

    // zmienne do przechowywania wartości próbek
    short lewy_wejscie, prawy_wejscie, lewy_wyjscie, prawy_wyjscie;

    // Przetwarzanie próbek sygnału w pętli
    while (1) {

        // odczyt próbek audio, zamiana na mono
        aic3204_codec_read(&lewy_wejscie, &prawy_wejscie);
        short mono_wejscie = (lewy_wejscie >> 1) + (prawy_wejscie >> 1);

        // sprawdzamy czy wciśnięto przycisk
        // argument: maksymalna liczba obsługiwanych trybów
        tryb_pracy = pushbuttons_read(4);
        if (tryb_pracy == 0) // oba wciśnięte - wyjście
            break;
        else if (tryb_pracy != poprzedni_tryb_pracy) {
            // nastąpiła zmiana trybu - wciśnięto przycisk

            for(i = 0; i < N; i++)
            {
                bufor_roboczy[i] = 0;
            }
            USBSTK5515_ULED_setall(0x0F); // zgaszenie wszystkich diód
            if (tryb_pracy == 1) {
                // wypisanie informacji na wyświetlaczu
                oled_display_message("PROJEKT ZPS        ", "TRYB 1             ");
                // zapalenie diody nr 1
                USBSTK5515_ULED_on(0);
            } else if (tryb_pracy == 2) {
                oled_display_message("PROJEKT ZPS        ", "TRYB 2             ");
                USBSTK5515_ULED_on(1);
            } else if (tryb_pracy == 3) {
                oled_display_message("PROJEKT ZPS        ", "TRYB 3             ");
                USBSTK5515_ULED_on(2);
            } else if (tryb_pracy == 4) {
                oled_display_message("PROJEKT ZPS        ", "TRYB 4             ");
                USBSTK5515_ULED_on(3);
            }
            // zapisujemy nowo ustawiony tryb
            poprzedni_tryb_pracy = tryb_pracy;
        }


        // zadadnicze przetwarzanie w zależności od wybranego trybu pracy
        tryb_pracy=4;
        if (tryb_pracy == 1)
        {
            lewy_wyjscie = mono_wejscie;
            prawy_wyjscie = mono_wejscie;

        }
        else if (tryb_pracy == 2)
        {
            rand16((DATA *)&lewy_wyjscie,1);
            iircas51((DATA *)&lewy_wyjscie, (DATA *)IIR_wspolczynniki, (DATA *)&lewy_wyjscie, (DATA *)bufor_roboczy, 3, 1);

            lewy_wyjscie >>= 4;
            prawy_wyjscie = lewy_wyjscie;




        }
        else if (tryb_pracy == 3)
        {
            lewy_wyjscie = mono_wejscie;
            prawy_wyjscie = mono_wejscie;

            lewy_wyjscie = (akumulator_fazy);
            iircas51((DATA *)&lewy_wyjscie, (DATA *)IIR_wspolczynniki, (DATA *)&lewy_wyjscie, (DATA *)bufor_roboczy, 4, 1);

            lewy_wyjscie >>= 4;

            prawy_wyjscie = lewy_wyjscie;

        }
        else if (tryb_pracy == 4)
        {
            lewy_wyjscie = mono_wejscie;
            prawy_wyjscie = mono_wejscie;
            iircas51((DATA *)&mono_wejscie, (DATA *)IIR_wspolczynniki, (DATA *)&lewy_wyjscie, (DATA *)bufor_roboczy, 4, 1);

        }

        akumulator_fazy += KROK_FAZY_PILO;

        bufor[index] = lewy_wyjscie;

        index++;

        if(index == X)
        {


                index = 0; //tu breakpoint do wykresu
        }

        // zapisanie wartości na wyjście audio
        aic3204_codec_write(lewy_wyjscie, prawy_wyjscie);

    }


    // zakończenie pracy - zresetowanie kodeka
    aic3204_disable();
    oled_display_message("KONIEC PRACY       ", "                   ");
    while (1);
}
