from pathlib import Path
import csv,json,hashlib

W=Path(__file__).resolve().parent
REL=Path('mods/VanillaDeluxe/VanillaDeluxe.mpq/data/global/excel')
SOURCE=W/'source/full_archive'/REL

def read(path):
    with path.open(encoding='utf-8-sig',newline='') as f:
        reader=csv.DictReader(f,delimiter='\t')
        return reader.fieldnames,list(reader)

def gate(tier):return f'VDML Sustain T{tier} Gate'

def build():
    changes=[];counts=[]
    for bank in ['', 'base/']:
        h,rows=read(SOURCE/bank/'treasureclassex.txt')
        m={r['Treasure Class']:r for r in rows}
        assert len(m)==len(rows)
        previous_count=len(rows)
        for tier in range(1,7):
            r=m[f'RMap T{tier} Sustain']


            for i in range(1,11):
                k='Item'+str(i)
                if r[k].startswith('RMap Tier '):
                    before=r[k];r[k]=gate(int(before[-1]))
                    changes.append({'bank':bank or 'main','row':r['Treasure Class'],
                                    'field':k,'before':before,'after':r[k]})
            r=m[f'RMap T{tier} Gilded Bonus']
            assert r['Item3']=='VDML Empty' and r['Prob3']=='2'
            assert r['Item4']=='mrl' and r['Prob4']=='2'
            r['Prob3']='3';r['Prob4']='1'
            changes.append({'bank':bank or 'main','row':r['Treasure Class'],
                            'fields':{'Prob3':['2','3'],'Prob4':['2','1']}})
        r=m['RMap Currency']
        assert r['Item1']=='VDML Empty' and r['Prob1']=='5'
        assert r['Item2']=='mrl' and r['Prob2']=='4'
        r['Prob1']='7';r['Prob2']='2'
        changes.append({'bank':bank or 'main','row':'RMap Currency',
                        'fields':{'Prob1':['5','7'],'Prob2':['4','2']}})
        gates=[]
        for tier in range(1,6):
            name=gate(tier);assert name not in m
            row={k:'' for k in h}
            row.update({'Treasure Class':name,'Picks':'1','NoDrop':'0',
                        'Item1':f'RMap Tier {tier}','Prob1':'1',
                        'Item2':'VDML Empty','Prob2':'3','*eol':'0'})
            gates.append(row)




        insertion=next(i for i,r in enumerate(rows) if r['Treasure Class']=='RMap Currency')+1
        rows[insertion:insertion]=gates
        counts.append({'bank':bank or 'main','before':previous_count,'after':len(rows)})
        out=W/'package'/REL/bank/'treasureclassex.txt';out.parent.mkdir(parents=True,exist_ok=True)
        with out.open('w',encoding='utf-8',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=h,delimiter='\t',lineterminator='\r\n')
            writer.writeheader();writer.writerows(rows)
    (W/'changes.json').write_text(json.dumps(changes,indent=2))
    (W/'package/INSTRUKCJA.txt').write_text('''Mapy — ograniczenie dropu map i Vizjerei Prism

Zmiany:
- Losowe mapy z potworów w mapach T1-T6: o75% rzadsze.
- Gwarantowana jedna mapa z Wardena pozostaje bez zmian.
- Vizjerei Prism: o50% rzadszy we wszystkich mapowych źródłach, również
  w losowaniu currency Wardena i w bonusie Gilded.
- Szansa na Corrupted Worldstone Shard, zwykły loot, pozostałe materiały,
  jakość przedmiotów i9 losowań zwykłego lootu Wardena zachowane.
- Heraldzi, Baal i potwory poza mapami mają dotychczasowe szanse.

Szacowany wynik: przy wcześniejszych6-8 mapach łącznie, w tym jednej z Wardena,
średnia po zmianie wynosi około2,25-2,75. To cel balansu oparty o Twoje wyniki,
nie limit ani gwarancja w pojedynczej rozgrywce. Density i liczba graczy nadal
wpływają na liczbę łupów. Rozkład tierów i lokacji losowych map pozostaje taki sam.

Instalacja:
1. Zamknij grę i skopiuj folder mods do katalogu gry, zastępując dwa pliki
   treasureclassex.txt (główny oraz w podfolderze base).
2. W konsoli Loadera wykonaj:

   d2rl compile VanillaDeluxe

3. Po zakończeniu kompilacji całkowicie zamknij grę, uruchom ją ponownie
   i utwórz nową rozgrywkę.
4. Na LAN zainstaluj i skompiluj tę samą paczkę na obu komputerach.

Paczka zawiera tylko tabele dropów. Nie podmienia MonStats, HP, modeli,
Cubemain, Properties ani DLL. Zachowuje recepturę3 odłamki + niebieska mapa,
naprawę Essence i poprzednie poprawki map. Nie instaluj na nią starszego
treasureclassex.txt, ponieważ przywróci poprzednie szanse.

Weryfikacja plików i obliczenia prawdopodobieństw wykonane lokalnie.
Nie uruchomiono gry ani kompilatora d2rl. Średnią liczbę map oceń po kilku
pełnych przejściach przy podobnym zaludnieniu, nie po jednym dropie.
''',encoding='utf-8')
    print(json.dumps({'banks':counts,'edited_existing_rows_per_bank':13,'new_gates_per_bank':5}))

if __name__=='__main__':build()
