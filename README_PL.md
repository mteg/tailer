> tailer (plural tailers)
> 1. One who follows or tails surreptitiously, as an investigator
> 2. (...)

# Problem

Jak zapisać log sesji np. telnetu?

Można zrobić tak:

```
telnet 172.30.32.254 | tee telnet.log
```

I to nawet działa, dopóki się nie użyje kursora (np. aby poprawić literówkę). Bo log wygląda wtedy tak:

```
C4_Szombierki# shw cableESC[1DESC[1DESC[1DESC[1DESC[1DESC[1DESC[1D
ESC[1Dohw cableESC[8DESC[1Dhw cable ESC[9DESC[1Cow cableESC[7D
Error: Enter show cable ? to obtain help
```

Tailer rozwiązuje ten problem.

# Porządkowanie logów zapisanych poprzez script/tee
Log z poleceniami terminala można przy pomocy tailera łatwo "uporządkować". Tailer interpretuje odpowiednio kody terminala, wprowadzając poprawki w tekście logu. Kody są usuwane i nie są widoczne w końcowym produkcie.

```
$ tailer < telnet.log | grep cable
C4_Szombierki# show cable
Error: Enter show cable ? to obtain help
```

# Interaktywne logowanie
Tailer może również działać interaktywnie, zapisując do logu każdą linię ze zinterpretowanymi i usuniętymi kodami terminala.

```
$ tailer -tf "tailer.log" -- ssh mteg@lab.jaszczur.org -p 60022
```

# Opcje
```
$ ./tailer -h
Usage: ./tailer [options] [-f <output file>] [-- <command> [arguments]]
   -a       Append instead of creating a new file
   -W <col> Override terminal width
   -H <row> Override terminal height
   -i       Ignore resizes
   -p <pfx> Prefix all lines with the specified string
   -t       Add timestamp to every line
```

# Jak działa tailer?
Tailer prowadzi własny, wirtualny terminal (w pamięci, nie wyświetlając go na ekranie).  Formalnie jest to terminal typu ANSI.

Każdy znak z wejścia jest wysyłany na ten terminal. W momencie kiedy wykryty zostaje znak LF ('\n', 0x0a), zawartość terminala jest zrzucana do pliku
od pierwszej linii aż do pozycji kursora. Spacje z końcówki każdej linii są wycinane (`rtrim`). 
Puste linie są ignorowane. Wirtualny terminal jest w tym momencie czyszczony.   

# Known issues
- Tailer działa linia-po-linii
    - Po każdym wykrytym enterze wirtualny terminal jest czyszczony, więc aplikacje typu "tekstowe GUI" (np. mc) nie będą wyglądały za dobrze w logu tailera
- Maksymalna długość linii równa jest pojemności terminala w znakach (np. 80x25 = 2000 znaków). Jeśli wklei się 100 kB tekstu bez entera to znaczna część umknie tailerowi; do pliku zostanie zrzucona tylko końcówka.
