# tetris-de10-standard
Jogo Tetris desenvolvido em C para a placa DE10-Standard (ARM Cortex-A9) utilizando o Altera Monitor Program.

Este repositório contém o código-fonte em C de uma implementação do clássico jogo **Tetris**, desenvolvido para rodar de forma nativa (*bare-metal*) no processador **ARM Cortex-A9** presente no Hard Processor System (HPS) do FPGA Cyclone V na placa **DE10-Standard**.

Trabalho prático desenvolvido para a disciplina de **Arquitetura de Alto Desempenho** na **UFSCar**.

Para executar o jogo, você precisará criar o projeto localmente no **Altera Monitor Program**, associando estes arquivos de código aos arquivos de configuração de hardware do sistema **DE10-Standard Computer**.

#### Passo 1: Compilar o Código C
1. Abra o **Altera Monitor Program** e vá em *File > New Project*.
2. Escolha o seu diretório de trabalho, dê um nome ao projeto e selecione a arquitetura **ARM Cortex-A9**.
3. Na tela de sistema (*System Details*), selecione o arquivo `.sopcinfo` e o `.sof` do *DE10-Standard Computer*.
4. Na tela de *Program Type*, selecione **C Program**.
5. Adicione os arquivos fonte do repositório: `tetris.c` e `video.c` (os arquivos `.h` serão incluídos de forma automática por estarem na mesma pasta).
6. Avance mantendo as configurações de memória padrão (*Basic*) e clique em **Save**.
7. Clique em **Compile**. O executável binário **`tetris.srec`** terá sido gerado com sucesso na pasta do seu projeto. 

#### Passo 2: Gravar e Executar na Placa
1. Crie um **Novo Projeto** (*File > New Project*).
2. Configure exatamente o mesmo nome, diretório e as mesmas configurações de hardware do Passo 1.
3. Na tela de *Program Type*, altere a opção para **"AXF, ELF or SREC File"** (em vez de *C Program*).
4. Na tela seguinte, em *Source File*, aponte diretamente para o arquivo **`tetris.srec`** que foi gerado no Passo 1.
5. Conclua o assistente e clique em **Load**. O Monitor Program irá carregar o hardware no FPGA, enviar o binário compilado para a memória DDR3 da placa e iniciar o jogo no monitor VGA sem travar.
