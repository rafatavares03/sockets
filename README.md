<h1 align="center">Sockets</h1>
<p align="center">Cliente e servidor HTTP implementados em C.</p>

Sobre
-----
Esse projeto é um trabalho da disciplina de redes de computadores em que é necessário implementar dois sockets: um cliente e um servidor HTTP.
O socket cliente é capaz de fazer requisições do tipo GET e baixar o conteúdo em uma pasta.
O servidor, consegue servir arquivos de uma determinada pasta para a rede atráves de requisições HTTP.

Como Usar
---------
Primeiramente você deve clonar esse repositório e em seguida utilizar o comando: `make`. Ou se preferir compilar cada socket separadamente, execute: 
`gcc servidor/main.c -o ./server` e `gcc cliente/main.c -o ./client`.

Com os sockets já compilados, vamos para a execução de cada um. Para o cliente você deve executar o arquivo `./client` junto com uma URL.
```
./client http://site.com
./client http://127.0.0.1:8080/arquivo.txt
```
Caso seja especificado um caminho de arquivo que contenha espaços (' '), a URL deve ser envolvida com aspas duplas: `"http://127.0.0.1:8080/meu arquivo.pdf"`.

Para executar o servidor é necessário apenas executar o arquivo `./server` junto com um caminho de um diretório.
```
./server ../Documents/arquivos
```

Limitações
----------
É possível executar o cliente com uma URL `https://`, porém a requisição enviada será uma requisição HTTP. Portanto se o endereço especificado não suportar requisições HTTP,
o conteúdo desejado poderá não ser baixado.

O servidor atende apenas a requisições do tipo GET, caso outro método seja enviado ele retornará 405.

Autor
------
<h3 align="center">Rafael Tavares</h3>
