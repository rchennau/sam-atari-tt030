==========================================================================
XBoot V.3.10  README.TXT                                 29. November 1993
==========================================================================

nderungen und Erweiterungen, die nicht im Handbuch beschrieben sind:

--------------------------------------------------------------------------
Maximal 6 Accessories (Parameter-Dialog)                       Kapitel 5.6
--------------------------------------------------------------------------
ù Hier l„t sich jetzt einstellen, ob XBoot die Auswahl auf maximal sechs 
Accessories beschr„nkt oder nicht. Diese Option ist in erster Linie fr 
Anwender gedacht, die Erweiterungen wie MultiGEM oder das neue MultiTOS 
benutzen, bei denen eine Beschr„nkung auf sechs Accessories nicht mehr
besteht.

--------------------------------------------------------------------------
Nicht vorhandene Dateien in Sets                               Kapitel 8.3
--------------------------------------------------------------------------
ù Bei der Auswahl eines Sets wird nun berprft, ob die im Set verzeich- 
neten PRGs/ACCs/CPX-Module auch tats„chlich (noch) existieren. Falls 
nicht, wird eine Meldung ausgegeben und die Datei aus dem Set entfernt. 
So kann es keine "Leichen" von Dateien geben, die Platz in einem Set 
belegen, obwohl es sie gar nicht mehr gibt.

--------------------------------------------------------------------------
Fernsteuerung ber Kommandozeile
--------------------------------------------------------------------------
ù Wird XBoot beim Start vom Desktop der Name eines Sets als Parameter in
der Kommandozeile bergeben, dann wird ein Reset ausgel”st, und das be-
treffende Set wird automatisch installiert: XBoot startet nach dem Reset 
sofort "durch" und installiert das Set. Eine ggf. aktivierte Pawort- 
Abfrage wird dabei nicht durchgefhrt.

Wichtig: Set-Namen, in denen Leerzeichen vorkommen, mssen bei der 
šbergabe in Anfhrungszeichen eingeschlossen werden!

Diese Funktion richtet sich vornehmlich an erfahrene Anwender, die XBoot 
z.B. aus eigenen Programmen heraus fernsteuern m”chten. Fr den 
"allt„glichen" Gebrauch eignet sich dagegen folgendes neue Feature:

--------------------------------------------------------------------------
Start vom Desktop
--------------------------------------------------------------------------
ù Wird XBoot vom Desktop gestartet, und ist beim Verlassen des Programms 
ein Set selektiert, fragt XBoot jetzt, ob das Set sofort installiert 
werden soll. Beantwortet man die Frage mit Ja, wird ein Reset ausgel”st 
und das Set installiert (s.o.).

Kleiner Tip: Ziehen Sie XBOOT.PRG als Icon auf den Desktop (geht erst ab 
TOS-Version 2.x). So k”nnen Sie XBoot jederzeit bequem vom Desktop aus 
aufrufen, um den Rechner neu zu booten und ein neues Set zu installieren. 
Damit ist es jetzt also sehr einfach geworden, schnell die aktuelle 
Bildschirmaufl”sung zu wechseln oder die fr ein bestimmtes Programm 
ben”tigten residenten Hilfsprogramme aus dem AUTO-Ordner zu laden.

Vergessen Sie aber nicht, diese Einstellungen mittels "Arbeit sichern" 
abzuspeichern. Das Icon und seine Bildschirmposition werden in der Datei 
NEWDESK.INF gespeichert. Sollten Sie also in XBoot mehrere NEWDESK.INF- 
Dateien verwalten, mssen Sie danach natrlich noch die neu gespeicherte 
NEWDESK.INF unter dem Namen des entsprechenden "Originals" abspeichern. 
Die genaue Vorgehensweise zur Erstellung und Verwaltung mehrerer 
alternativer NEWDESK.INF-Dateien wird im Handbuch-Kapitel ber 
Infodateien ausfhrlich beschrieben.

--------------------------------------------------------------------------
Cut/Copy/Paste im Batch-Editor                                 Kapitel 4.5
--------------------------------------------------------------------------
ù Der Batch-Editor untersttzt nun Cut/Copy/Paste, d.h. die Funktionen 
Ausschneiden, Kopieren und Einfgen. Die Operationen beziehen sich immer 
auf die gesamte Zeile, in der sich der Cursor befindet. Damit ist es nun 
z.B. sehr leicht m”glich, l„ngere Befehlszeilen zwischen verschiedenen 
Sets zu kopieren. Es gelten die Standard-Tastenbelegungen:

Ctrl + X         Schneidet aktuelle Zeile aus und speichert sie in
                 der Zwischenablage
Ctrl + C         Kopiert aktuelle Zeile in die Zwischenablage
Ctrl + V         Fgt Zeile aus Zwischenablage ein

--------------------------------------------------------------------------
Programmflags im Fileselector                                Kapitel 5.9.1
--------------------------------------------------------------------------
ù Der Fileselector erlaubt jetzt auch das ndern der Speicherschutzflags 
eines Programms unter MultiTOS. Diese Flags legen fest, ob und in 
welchem Umfang der vom Programm belegte Speicher vor dem Zugriff durch 
fremde Programme (Prozesse) geschtzt ist.

Die Standardeinstellung ist Private und sollte nur in Ausnahmef„llen 
ge„ndert werden, etwa wenn ein (insbesondere „lteres) Programm nicht 
korrekt unter MultiTOS funktioniert. Versuchen Sie dann schrittweise, den 
Speicherschutz des Programms auf World-readable, Super und schlielich 
auf Global zu „ndern.

Private           -  Nur der Proze selbst (und MultiTOS) k”nnen auf den
                     Speicher zugreifen (Standardeinstellung).
Global            -  Speicher ist komplett ungeschtzt.
Super             -  Zugriff nur im Supervisormodus m”glich.
World-readable    -  Zugriff nur lesend m”glich

Anmerkung: Die Funktion ist in erster Linie fr erfahrene Anwender 
gedacht. ndern Sie die o.g. Flags also nur in Ausnahmef„llen.

