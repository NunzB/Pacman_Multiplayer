# Pacman_Multiplayer
This was a project I developed within the scope of the "Systems Programming" Course at Instituto Superior Técnico in Lisboa. The project assignment and the project report are available on the repository.

The code can be run on mac and Linux, using the terminal. 

1) To start the server issue the command: "./Server X Y", in which X and Y are the numbers of columns and lines, respectively.
2) An empty table opens, and you are supposed to place bricks by left-clicking with the cursor on the desired position. When you're done, close the game window.
3) A new window with the brick position opens. You have now the server available to receive socket connections from clients. To connect a client issue on the terminal "./Client IP PORT COLOUR", where IP is the server address (you can use 127.0.0.1 if in the same machine), PORT is number 3000, and COLOUR can be r, g or b for red, green and blue, respectively.
4) Connect as many clients as you'd like, and enjoy!

