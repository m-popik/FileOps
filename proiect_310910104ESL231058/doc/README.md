Rolul directoarelor din proiect:

    1. “doc/” contine fisierele markdown ce explica si documenteaza proiectul si un sumar al comenzilor
    2. “tools/” contine utilitare ce sunt folosite pentru procesarea sarcinilor din proiect
    3. “reports/” contine outputul fiecarui script din cerinta 2

Cum functioneaza auditul?

1. adauga la finalul fisierului (face append la) T1_comenzi.md data si ora la care a pornit scriptul
2. incearca sa creeaze directoarele din reports daca nu exista. Daca exista, returneaza eroare dar outputul comenzilor inca se scrie in fisierul aferent
3. executa comenzile de la cerinta 2 
4.adauga la finalul fisierului T1_comenzi.md data si ora la care scriptul si-a terminat executia

rapoartele sunt salvate in fisierul “reports/”, ce contine 4 fisiere ce corespund subcerintelor de la punctul 2)
