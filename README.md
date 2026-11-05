# Competição de Ordenação: O Desafio dos Milissegundos

Manual prático passo a passo para instalar as ferramentas necessárias, preparar os arquivos, compilar e executar o programa de ordenação em **Windows**, **Linux** e **macOS**, mesmo que você nunca tenha programado em linguagem C antes.

---

## 1. Sobre o projeto

Este projeto contém um programa em linguagem C ([`ordenacao.c`](ordenacao.c)) desenvolvido para a atividade acadêmica **"Competição de Ordenação: O Desafio dos Milissegundos"**.

O objetivo do programa é ler uma lista com cerca de 10 milhões de números inteiros a partir do arquivo **`listaOdenada.txt`**, ordená-los em ordem crescente no menor tempo possível utilizando uma estratégia **Híbrida Adaptativa** (**Counting Sort** e **Radix Sort LSD**), medir exclusivamente o tempo gasto na ordenação com a função `clock()` e verificar automaticamente se o resultado final está correto.

---

## 2. Requisitos necessários

Para usar este projeto em qualquer computador, você precisará apenas de:

1. **Um computador com Windows (10 ou 11), Linux ou macOS.**
2. **Cerca de 500 MB de memória RAM livre** durante a execução do programa.
3. **Um compilador de linguagem C instalado** (ensinado passo a passo nas seções 3, 4 e 5 abaixo).
4. **Os arquivos do projeto** (`ordenacao.c`) e o arquivo de dados da atividade chamado exatamente **`listaOdenada.txt`** (~60 MB).

---

## 3. Instalação no Windows

Se você usa **Windows 10** ou **Windows 11**, a maneira mais simples de instalar o compilador C (`gcc`) é através do pacote gratuito **WinLibs (MinGW-w64)** ou via **MSYS2**. Abaixo está o passo a passo completo usando o **MSYS2** (instalador oficial com interface gráfica).

### Passo 1: Baixar o instalador oficial
1. Abra o seu navegador de internet e acesse o site oficial do **MSYS2**:
   https://www.msys2.org/
2. Na página inicial, clique no botão de download do instalador (arquivo terminado em **`.exe`**, por exemplo: `msys2-x86_64-latest.exe`).

### Passo 2: Instalar o programa
1. Abra o arquivo `.exe` que você acabou de baixar.
2. Clique em **Avançar (Next)** mantendo a pasta padrão `C:\msys64`.
3. Continue clicando em **Avançar (Next)** até finalizar a instalação.
4. Ao final, mantenha marcada a opção **"Run MSYS2 now"** e clique em **Concluir (Finish)**. Uma janela preta de terminal se abrirá.

### Passo 3: Instalar o compilador C (GCC)
1. Na janela preta do MSYS2 que se abriu, copie o comando abaixo, clique com o botão direito dentro da janela e escolha **Paste** (ou pressione `Shift + Insert`), e aperte a tecla **Enter**:
   ```bash
   pacman -S --needed base-devel mingw-w64-ucrt-x86_64-gcc
   ```
2. Quando aparecer a pergunta `Enter a selection (default=all):`, apenas aperte **Enter**.
3. Quando aparecer a pergunta `Proceed with installation? [Y/n]`, digite **`Y`** e aperte **Enter**.
4. Aguarde o download e a instalação terminarem e feche a janela.

### Passo 4: Adicionar o compilador ao Windows (Variáveis de Ambiente)
Para que o Windows reconheça o comando `gcc` em qualquer pasta:
1. Aperte a tecla **Windows** no seu teclado e digite: **` variáveis de ambiente `**
2. Clique na opção **"Editar as variáveis de ambiente do sistema"**.
3. Na janela que abrir, clique no botão **"Variáveis de Ambiente..."** (na parte inferior).
4. Na caixa de baixo (**"Variáveis do sistema"**), procure a linha chamada **`Path`**, clique nela uma vez e clique em **"Editar..."**.
5. Clique no botão **"Novo"** (à direita) e cole exatamente este caminho:
   ```text
   C:\msys64\ucrt64\bin
   ```
6. Clique em **OK** em todas as janelas abertas para salvar.

### Passo 5: Verificar se a instalação funcionou no Windows
1. Aperte a tecla **Windows**, digite **`cmd`** e clique em **Prompt de Comando**.
2. Digite o comando abaixo e aperte **Enter**:
   ```cmd
   gcc --version
   ```
3. Se aparecer uma mensagem mostrando a versão (por exemplo, `gcc.exe (Rev...) 13.x.x` ou `14.x.x`), o compilador está pronto para uso!

---

## 4. Instalação no Linux

No Linux, a instalação é feita diretamente pelo terminal usando o gerenciador de pacotes oficial da sua distribuição.

### Passo 1: Abrir o Terminal no Linux
- Pressione as teclas **`Ctrl + Alt + T`** juntas no teclado, ou procure por **Terminal** no menu de aplicativos do seu sistema.

### Passo 2: Instalar o compilador C (GCC)

Escolha abaixo o comando correspondente à sua distribuição Linux, copie, cole no terminal (com `Ctrl + Shift + V` ou botão direito $\rightarrow$ **Colar**) e aperte **Enter**. O sistema pedirá a senha do seu usuário (digite a senha normalmente; por segurança, nada aparece na tela enquanto você digita) e aperte **Enter**.

- **Para Ubuntu, Linux Mint, Debian, Pop!_OS ou Zorin OS:**
  ```bash
  sudo apt update && sudo apt install -y build-essential
  ```

- **Para Fedora, Red Hat ou CentOS:**
  ```bash
  sudo dnf install -y gcc make
  ```

- **Para Arch Linux ou Manjaro:**
  ```bash
  sudo pacman -S --needed base-devel
  ```

### Passo 3: Verificar se a instalação funcionou no Linux
1. No mesmo terminal, digite o comando abaixo e aperte **Enter**:
   ```bash
   gcc --version
   ```
2. Se aparecer o texto `gcc (Ubuntu ...) ...` (ou o nome da sua distribuição) seguido do número da versão, tudo está funcionando corretamente.

---

## 5. Instalação no macOS

No macOS (qualquer MacBook ou iMac, tanto com processador **Apple Silicon M1/M2/M3/M4** quanto **Intel**), o compilador oficial da Apple (`clang` / `gcc`) é instalado através do pacote **Xcode Command Line Tools**.

### Passo 1: Abrir o Terminal no macOS
1. Pressione as teclas **`Command (⌘) + Espaço`** juntas para abrir a busca do Spotlight.
2. Digite **`Terminal`** e aperte a tecla **Return (Enter)**.

### Passo 2: Instalar o Xcode Command Line Tools
1. Copie o comando abaixo, cole na janela do Terminal (com `Command (⌘) + V`) e aperte **Return (Enter)**:
   ```bash
   xcode-select --install
   ```
2. Uma janela de aviso do macOS aparecerá na tela perguntando se você deseja instalar as ferramentas de linha de comando.
3. Clique no botão **"Instalar" (Install)** e depois em **"Concordo" (Agree)** nos termos de licença.
4. Aguarde a barra de progresso terminar o download e a instalação.

*(Nota: Caso você prefira baixar manualmente pelo site da Apple, as ferramentas também estão disponíveis em https://developer.apple.com/download/all/, mas o comando acima faz tudo sozinho).*

### Passo 3: Verificar se a instalação funcionou no macOS
1. Após concluir a instalação, volte à janela do **Terminal**, digite o comando abaixo e aperte **Return (Enter)**:
   ```bash
   clang --version
   ```
2. Se aparecer uma mensagem como `Apple clang version ...`, seu Mac já está pronto para compilar e rodar o projeto.

---

## 6. Como baixar e preparar os arquivos

### Passo 1: Baixar o projeto do GitHub (em formato ZIP)
1. Acesse a página oficial deste repositório no seu navegador:
   https://github.com/BrenoBressiani/desafio-dos-milissegundos-ordenacao
2. Clique no botão verde **`<> Code`** (no canto superior direito da lista de arquivos).
3. Clique na última opção: **"Download ZIP"**.
4. Vá até a sua pasta de **Downloads** e extraia (descompacte) o arquivo `.zip` baixado:
   - **No Windows:** Clique com o botão direito sobre o arquivo `.zip` $\rightarrow$ **"Extrair Tudo..."** $\rightarrow$ clique em **"Extrair"**.
   - **No Linux:** Clique com o botão direito sobre o arquivo `.zip` $\rightarrow$ **"Extrair aqui"** (*Extract Here*).
   - **No macOS:** Dê dois cliques sobre o arquivo `.zip` que o macOS criará automaticamente a pasta extraída.

*(Opcional — se você já usa Git e preferir clonar via terminal, basta rodar: `git clone https://github.com/BrenoBressiani/desafio-dos-milissegundos-ordenacao.git`).*

### Passo 2: Colocar o arquivo `listaOdenada.txt` no lugar correto
> **MUITO IMPORTANTE:** O arquivo de dados da atividade (que tem cerca de 60 MB) tem a grafia exata **`listaOdenada.txt`** (sem a letra "r" depois do "O").

1. Pegue o arquivo **`listaOdenada.txt`** disponibilizado pelo professor.
2. Copie e cole o arquivo **`listaOdenada.txt`** **dentro da mesma pasta onde está o arquivo `ordenacao.c`**.
3. Olhe dentro da pasta do projeto: você deve ver lado a lado o arquivo **`ordenacao.c`** e o arquivo **`listaOdenada.txt`**.

### Passo 3: Como abrir o terminal diretamente dentro da pasta do projeto

- **No Windows (Windows 10 ou 11):**
  1. Abra a pasta do projeto no Explorador de Arquivos (onde estão `ordenacao.c` e `listaOdenada.txt`).
  2. Clique uma vez na **barra de endereço** no topo da janela (onde aparece o caminho da pasta), apague o texto, digite **`cmd`** e aperte **Enter**.
  3. O **Prompt de Comando** abrirá já posicionado dentro da pasta certa!
  4. Para ter certeza de que os arquivos estão lá, digite `dir` e aperte **Enter**: você verá `ordenacao.c` e `listaOdenada.txt` na lista.

- **No Linux:**
  1. Abra a pasta do projeto no gerenciador de arquivos.
  2. Clique com o botão direito em um espaço vazio dentro da pasta e escolha **"Abrir no Terminal"** (*Open in Terminal*).
  3. Para conferir se os arquivos estão lá, digite `ls` e aperte **Enter**: você verá `ordenacao.c` e `listaOdenada.txt`.

- **No macOS:**
  1. Abra o **Finder** e localize a pasta do projeto.
  2. Clique com o botão direito (ou toque com dois dedos no trackpad) sobre o ícone da pasta do projeto e clique em **"Novo Terminal na Pasta"** (*New Terminal at Folder*).
  *(Alternativa no Mac: abra o aplicativo **Terminal**, digite `cd ` com um espaço no final, arraste a pasta do projeto do Finder para dentro da janela do Terminal e aperte **Return/Enter**).*
  3. Para conferir se os arquivos estão lá, digite `ls` e aperte **Return (Enter)**: você verá `ordenacao.c` e `listaOdenada.txt`.

---

## 7. Como compilar

Compilar significa transformar o código-fonte `ordenacao.c` no programa executável otimizado para o processador da sua máquina.

Você pode fazer isso **direto pelo botão "Play" (`▷`) do VS Code** ou pelo **Terminal** do seu sistema operacional:

---

### Opção Rápida: Direto pelo Visual Studio Code (VS Code — Windows, Linux ou macOS)

O projeto já vem pré-configurado na pasta `.vscode` para compilar automaticamente com otimização máxima (`-O3`) dentro do **VS Code**:

1. Abra o **Visual Studio Code (VS Code)**.
2. No menu superior, clique em **File (Arquivo) $\rightarrow$ Open Folder... (Abrir Pasta...)** e selecione a pasta do projeto (onde estão `ordenacao.c` e `listaOdenada.txt`).
3. Na aba de **Extensões** (ícone de 4 quadradinhos na barra lateral esquerda, ou `Ctrl + Shift + X` / `Cmd + Shift + X`), instale a extensão **C/C++** (da Microsoft) ou **Code Runner**.
4. Clique no arquivo **`ordenacao.c`** na lista lateral esquerda para abri-lo na tela.
5. Clique no botão **Play (`▷`)** no canto superior direito da janela do VS Code (ou pressione `Ctrl + Alt + N` no Code Runner).
6. O VS Code irá compilar e executar o programa automaticamente usando o arquivo `listaOdenada.txt` e mostrará o resultado na parte inferior da tela!

*(Alternativa pelo Terminal do próprio VS Code: aperte **`Ctrl + '`** para abrir o terminal integrado na parte de baixo do VS Code e siga os comandos de compilação e execução da sua plataforma abaixo).*

---

### A) Compilar pelo Terminal no Windows

1. Com o **Prompt de Comando (`cmd`)** aberto dentro da pasta do projeto (conforme ensinado no Passo 3 da seção anterior), copie o comando abaixo:
   ```cmd
   gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao.exe ordenacao.c
   ```
2. Clique com o botão direito dentro da janela do Prompt de Comando para colar o comando e aperte **Enter**.
3. **Como saber se funcionou:** O comando termina em cerca de 1 a 2 segundos sem exibir mensagens de erro (`error`) e volta a mostrar o cursor de digitação.
4. **Arquivo gerado:** Será criado o programa executável **`ordenacao.exe`** dentro da pasta do projeto.

*(Nota para usuários avançados no Windows que utilizem o **Developer Command Prompt for Visual Studio** em vez do GCC: o comando equivalente é `cl /O2 /arch:AVX2 /W4 ordenacao.c /Fe:ordenacao.exe`).*

---

### B) Compilar no Linux

1. Com o **Terminal** aberto dentro da pasta do projeto, copie o comando abaixo:
   ```bash
   gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
   ```
2. Cole no terminal (`Ctrl + Shift + V` ou botão direito $\rightarrow$ **Colar**) e aperte **Enter**.
3. **Como saber se funcionou:** O terminal não mostra nenhuma mensagem de erro e retorna imediatamente para a linha de comando.
4. **Arquivo gerado:** Será criado o arquivo executável **`ordenacao`** dentro da pasta do projeto.

---

### C) Compilar no macOS (Apple Silicon M1/M2/M3/M4 ou Intel)

1. Com o aplicativo **Terminal** aberto dentro da pasta do projeto, copie o comando abaixo:
   ```bash
   clang -O3 -mcpu=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
   ```
2. Cole no Terminal (`Command (⌘) + V`) e aperte **Return (Enter)**.
3. **Como saber se funcionou:** O Terminal não exibe nenhuma mensagem de erro e volta para o cursor normal em 1 segundo.
4. **Arquivo gerado:** Será criado o arquivo executável **`ordenacao`** dentro da pasta do projeto, otimizado nativamente para o processador do seu Mac.

---

## 8. Como executar

Agora que o programa foi compilado, basta executá-lo no mesmo terminal aberto na pasta do projeto.

---

### A) Executar no Windows

- **Se o arquivo `listaOdenada.txt` estiver na mesma pasta do programa (modo padrão):**
  No **Prompt de Comando (`cmd`)**, copie, cole e aperte **Enter**:
  ```cmd
  ordenacao.exe
  ```
  *(Se você estiver usando o **PowerShell** em vez do `cmd`, digite: `.\ordenacao.exe`)*

- **Se o arquivo `listaOdenada.txt` estiver em outra pasta:**
  Informe o caminho do arquivo entre aspas logo após o nome do programa. Exemplo:
  ```cmd
  ordenacao.exe "C:\Users\SeuNome\Downloads\listaOdenada.txt"
  ```

- **Opcional — Se quiser salvar os números ordenados em um novo arquivo de texto:**
  Basta passar um segundo nome de arquivo ao final do comando:
  ```cmd
  ordenacao.exe listaOdenada.txt resultadoOrdenado.txt
  ```

---

### B) Executar no Linux

- **Se o arquivo `listaOdenada.txt` estiver na mesma pasta do programa (modo padrão):**
  No **Terminal**, copie, cole e aperte **Enter**:
  ```bash
  ./ordenacao
  ```

- **Se o arquivo `listaOdenada.txt` estiver em outra pasta:**
  Informe o caminho do arquivo após o comando. Exemplo:
  ```bash
  ./ordenacao "/home/seu_usuario/Downloads/listaOdenada.txt"
  ```

- **Opcional — Se quiser salvar os números ordenados em um novo arquivo de texto:**
  ```bash
  ./ordenacao listaOdenada.txt resultadoOrdenado.txt
  ```

---

### C) Executar no macOS

- **Se o arquivo `listaOdenada.txt` estiver na mesma pasta do programa (modo padrão):**
  No **Terminal**, copie, cole e aperte **Return (Enter)**:
  ```bash
  ./ordenacao
  ```

- **Se o arquivo `listaOdenada.txt` estiver em outra pasta:**
  Informe o caminho do arquivo após o comando. Exemplo:
  ```bash
  ./ordenacao "/Users/seu_usuario/Downloads/listaOdenada.txt"
  ```
  *(Dica no Mac: você também pode digitar `./ordenacao ` com um espaço no final, arrastar o arquivo `listaOdenada.txt` do Finder para dentro da janela do Terminal e apertar **Return/Enter**).*

- **Opcional — Se quiser salvar os números ordenados em um novo arquivo de texto:**
  ```bash
  ./ordenacao listaOdenada.txt resultadoOrdenado.txt
  ```

---

## 9. Como verificar o resultado

Logo após executar o programa com o arquivo `listaOdenada.txt`, o terminal mostrará um relatório igual a este:

```text
============================================================
  COMPETICAO DE ORDENACAO: O DESAFIO DOS MILISSEGUNDOS
============================================================
Arquivo de entrada : listaOdenada.txt
Elementos lidos    : 10000000
Algoritmo aplicado : Hibrido Adaptativo -> Counting Sort
Tempo de ordenação : 0.010000 segundos (10.000 ms)
------------------------------------------------------------
Ordem crescente    : OK (Confirmada)
Integridade dados  : OK (zero perdas/adulteracoes)
Valores repetidos  : SIM (7500000 ocorrencias adjacentes preservadas)
Verificação final  : OK
============================================================
```
*(Observação: o valor exibido na linha `Tempo de ordenação` varia de acordo com o processador de cada computador).*

### Como ter certeza de que tudo deu certo:
1. **`Elementos lidos : 10000000`** — Confirma que todos os 10 milhões de números do arquivo `listaOdenada.txt` foram carregados.
2. **`Tempo de ordenação`** — Mostra o tempo exclusivo gasto pelo processador para ordenar os números (em segundos e em milissegundos `ms`).
3. **`Ordem crescente : OK (Confirmada)`** — Confirma que a lista inteira está ordenada do menor para o maior número.
4. **`Integridade dados : OK (zero perdas/adulteracoes)`** — Confirma matematicamente que nenhum número foi perdido ou alterado durante a ordenação.
5. **`Verificação final : OK`** — Indica que o programa concluiu a tarefa com 100% de sucesso.

---

## 10. Solução de problemas

Abaixo estão os erros mais comuns que podem acontecer na primeira vez e como resolver cada um rapidamente:

### Problema 1: "gcc não é reconhecido como um comando interno ou externo" ou "command not found"
- **Causa:** O compilador C ainda não foi instalado ou o terminal foi aberto antes de concluir a instalação.
- **Solução no Windows:**
  1. Feche todas as janelas do Prompt de Comando e abra uma nova janela (as variáveis de ambiente só valem para janelas abertas depois de clicar em OK).
  2. Revise o **Passo 4 da Seção 3 (Instalação no Windows)** e confirme se o caminho `C:\msys64\ucrt64\bin` está adicionado na variável `Path`.
- **Solução no Linux:** Execute novamente o comando da **Seção 4 (Instalação no Linux)** correspondente à sua distribuição.
- **Solução no macOS:** Execute `xcode-select --install` no Terminal conforme a **Seção 5 (Instalação no macOS)**.

---

### Problema 2: `Erro: nao foi possivel abrir o arquivo 'listaOdenada.txt'.`
- **Causa:** O arquivo `listaOdenada.txt` não está na mesma pasta em que você abriu o terminal, ou está com o nome diferente.
- **Solução:**
  1. Verifique se o arquivo se chama exatamente **`listaOdenada.txt`** (repare que o nome original da atividade é escrito `Odenada`, sem a letra "r").
  2. No Windows, cuidado para não renomear o arquivo para `listaOdenada.txt.txt` (com `.txt` duplicado).
  3. Confirme se o arquivo foi copiado para a mesma pasta onde está o arquivo `ordenacao.c`.

---

### Problema 3: `gcc: error: ordenacao.c: No such file or directory` (Erro ao tentar compilar)
- **Causa:** O seu terminal não está aberto dentro da pasta onde está salvo o arquivo `ordenacao.c`.
- **Solução:** Siga o **Passo 3 da Seção 6** para abrir o terminal diretamente dentro da pasta do projeto. Antes de compilar, digite `dir` (no Windows) ou `ls` (no Linux e macOS) para confirmar que `ordenacao.c` aparece na listagem.

---

### Problema 4: No macOS, o comando `-march=native` dá erro no compilador Clang
- **Causa:** Em alguns modelos de Mac com chip Apple Silicon (M1/M2/M3/M4), o compilador `clang` da Apple utiliza o parâmetro `-mcpu=native` em vez de `-march=native`.
- **Solução no macOS:** Utilize o comando exato indicado na **Seção 7 (item C)**:
  ```bash
  clang -O3 -mcpu=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
  ```
  Se ainda assim a versão do seu Xcode for muito antiga, compile apenas com `-O3`:
  ```bash
  clang -O3 -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
  ```

---

### Problema 5: O programa não inicia ao digitar `ordenacao` no terminal
- **No Windows (PowerShell):** O PowerShell exige `.\` antes do nome do executável. Digite:
  ```powershell
  .\ordenacao.exe
  ```
- **No Linux ou macOS:** O terminal exige `./` antes do nome do executável. Digite:
  ```bash
  ./ordenacao
  ```
  Caso apareça `Permission denied` (Permissão negada) no Linux ou macOS, dê permissão de execução com o comando abaixo e tente novamente:
  ```bash
  chmod +x ordenacao
  ```

---

### Problema 6: `Erro: memoria insuficiente...` ou `Erro: falha de alocacao de memoria durante a ordenacao.`
- **Causa:** O computador está com a memória RAM quase totalmente ocupada por outros programas abertos.
- **Solução:** Feche abas pesadas do navegador ou outros aplicativos abertos para liberar pelo menos 200 MB a 500 MB de memória RAM e execute o programa novamente.

---

## 11. Estrutura dos arquivos do projeto

```text
desafio-dos-milissegundos-ordenacao/
├── .vscode/
│   ├── settings.json  # Configuração automática para o botão Play (Code Runner) no VS Code
│   └── tasks.json     # Tarefa de compilação otimizada (-O3) para Windows, Linux e macOS no VS Code
├── ordenacao.c        # Código-fonte completo do programa em linguagem C
├── README.md          # Este manual prático de instalação, compilação e uso
├── .gitignore         # Configuração para ignorar arquivos temporários no Git
└── listaOdenada.txt   # Arquivo de entrada (~60 MB) que você deve colocar nesta pasta antes de executar
```
