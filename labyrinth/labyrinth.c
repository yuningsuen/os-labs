#include "labyrinth.h"
#include "../testkit/testkit.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

void printUsage() {
  printf("Usage:\n");
  printf("  labyrinth --map map.txt --player id\n");
  printf("  labyrinth -m map.txt -p id\n");
  printf("  labyrinth --map map.txt --player id --move direction\n");
  printf("  labyrinth --version\n");
  printf("  labyrinth --help\n");
}

int main(int argc, char *argv[]) {
  // Variables to store parsed arguments
  char *map_file = NULL;
  char player_id = '\0';
  char *move_direction = NULL;

  // Define long options
  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'v'},
      {"map", required_argument, 0, 'm'},
      {"player", required_argument, 0, 'p'},
      {"move", required_argument, 0, 'M'},
      {0, 0, 0, 0} // End marker
  };

  int option_index = 0;
  int c;

  // Parse options
  while ((c = getopt_long(argc, argv, "hvm:p:M:", long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'h':
      printUsage();
      return 0;
    case 'v':
      printf("Labyrinth Game version 1.0\n");
      // Check if there are any remaining arguments after parsing
      if (optind < argc) {
        return 1;
      }
      return 0;
    case 'm':
      map_file = optarg;
      break;
    case 'p':
      player_id = optarg[0];
      break;
    case 'M':
      move_direction = optarg;
      break;
    case '?':
      // getopt_long already printed error message
      printUsage();
      return 1;
    default:
      printUsage();
      return 1;
    }
  }

  // Check for extra non-option arguments
  if (optind < argc) {
    printf("Error: Unexpected argument '%s'\n", argv[optind]);
    printUsage();
    return 1;
  }

  // Validate required arguments
  if (map_file == NULL || player_id == '\0') {
    printf("Error: --map and --player are required\n");
    printUsage();
    return 1;
  }

  if (!isValidPlayer(player_id)) {
    printf("Error: Invalid player ID '%c'\n", player_id);
    return 1;
  }

  // Load the labyrinth
  Labyrinth labyrinth = {0}; // Initialize to zero
  if (!loadMap(&labyrinth, map_file)) {
    printf("Error: Failed to load map from '%s'\n", map_file);
    return 1;
  }

  // Handle move command
  if (move_direction != NULL) {
    if (!movePlayer(&labyrinth, player_id, move_direction)) {
      printf("Error: Cannot move player '%c' in direction '%s'\n", player_id,
             move_direction);
      return 1;
    }
    // Save the updated map back to file
    if (!saveMap(&labyrinth, map_file)) {
      printf("Error: Failed to save map to '%s'\n", map_file);
      return 1;
    }
  }

  // Print the current map state
  for (int i = 0; i < labyrinth.rows; i++) {
    printf("%s", labyrinth.map[i]);
  }

  return 0;
}

bool isValidPlayer(char playerId) {
  // TODO: Implement this function
  if (playerId >= '0' && playerId <= '9')
    return true;
  return false;
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
  // TODO: Implement this function
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Error opening file");
    return false;
  }
  char buffer[270];
  int i = 0;
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    // printf("%s", buffer);
    size_t length = strlen(buffer);
    strcpy(labyrinth->map[i++], buffer);
    labyrinth->cols = max(labyrinth->cols, length);
  }
  labyrinth->rows = i;
  fclose(file);
  return true;
}

Position findHelper(Labyrinth *labyrinth, char target) {
  Position pos = {-1, -1};
  for (int i = 0; i < labyrinth->rows; ++i) {
    for (int j = 0; j < labyrinth->cols; ++j) {
      if (labyrinth->map[i][j] == target) {
        pos.row = i;
        pos.col = j;
        break;
      }
    }
  }
  return pos;
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
  // TODO: Implement this function
  Position pos = findHelper(labyrinth, playerId);
  return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
  // TODO: Implement this function
  Position pos = findHelper(labyrinth, '.');
  return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
  // TODO: Implement this function
  if (labyrinth->map[row][col] == '.')
    return true;
  return false;
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
  // TODO: Implement this function
  Position pos = findPlayer(labyrinth, playerId);
  if (pos.row == -1 && pos.col == -1) {
    Position firstEmpty = findFirstEmptySpace(labyrinth);
    if (firstEmpty.row == -1 && firstEmpty.col == -1)
      return false;
    else {
      pos = firstEmpty;
      labyrinth->map[pos.row][pos.col] = playerId;
      return true;
    }
  } else {
    if (strcmp(direction, "up") == 0 && pos.row > 0 &&
        isEmptySpace(labyrinth, pos.row - 1, pos.col)) {
      labyrinth->map[pos.row][pos.col] = '.';
      labyrinth->map[pos.row - 1][pos.col] = playerId;
      return true;
    } else if (strcmp(direction, "down") == 0 &&
               pos.row < labyrinth->rows - 1 &&
               isEmptySpace(labyrinth, pos.row + 1, pos.col)) {
      labyrinth->map[pos.row][pos.col] = '.';
      labyrinth->map[pos.row + 1][pos.col] = playerId;
      return true;
    } else if (strcmp(direction, "left") == 0 && pos.col > 0 &&
               isEmptySpace(labyrinth, pos.row, pos.col - 1)) {
      labyrinth->map[pos.row][pos.col] = '.';
      labyrinth->map[pos.row][pos.col - 1] = playerId;
      return true;
    } else if (strcmp(direction, "right") == 0 &&
               pos.col < labyrinth->cols - 1 &&
               isEmptySpace(labyrinth, pos.row, pos.col + 1)) {
      labyrinth->map[pos.row][pos.col] = '.';
      labyrinth->map[pos.row][pos.col + 1] = playerId;
      return true;
    }
  }
  return false;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
  // TODO: Implement this function
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("Error opening file for writing");
    return false;
  }

  for (int i = 0; i < labyrinth->rows; i++) {
    fputs(labyrinth->map[i], file); // Write the string
    // fputc('\n', file);              // Add newline manually
  }

  fclose(file);
  return true;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col,
         bool visited[MAX_ROWS][MAX_COLS]) {
  // Check bounds and if already visited or not an empty space
  if (row < 0 || row >= labyrinth->rows || col < 0 || col >= labyrinth->cols ||
      visited[row][col] || labyrinth->map[row][col] != '.') {
    return;
  }

  // Mark as visited
  visited[row][col] = true;

  // Explore all four directions
  dfs(labyrinth, row - 1, col, visited); // up
  dfs(labyrinth, row + 1, col, visited); // down
  dfs(labyrinth, row, col - 1, visited); // left
  dfs(labyrinth, row, col + 1, visited); // right
}

bool isConnected(Labyrinth *labyrinth) {
  // TODO: Implement this function
  Position firstEmpty = findFirstEmptySpace(labyrinth);
  if (firstEmpty.row == -1 && firstEmpty.col == -1) {
    return true;
  } else {
    bool visited[MAX_ROWS][MAX_COLS] = {0};
    dfs(labyrinth, firstEmpty.row, firstEmpty.col, visited);
    for (int i = 0; i < labyrinth->rows; ++i) {
      for (int j = 0; j < labyrinth->cols; ++j) {
        if (labyrinth->map[i][j] == '.' && visited[i][j] == false) {
          return false;
        }
      }
    }
  }
  return true;
}
