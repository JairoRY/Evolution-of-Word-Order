install.packages("fmsb")
# Load required packages
library(ggplot2)
library(readr)
library(dplyr)
library(tidyr)
library(scales)
library(fmsb) 

# Read both CSV files
ModernEnglish <- read_csv("word_order_stats_Susanne.csv")
OldEnglish <- read_csv("word_order_stats_PCEEC.csv")

# Add corpus label
ModernEnglish$Corpus <- "ModernEnglish"
OldEnglish$Corpus <- "OldEnglish"

# Combine for comparison
all_data <- bind_rows(ModernEnglish, OldEnglish)

# 1. BAR CHART: Word order distribution per corpus
ggplot(all_data, aes(x = Order, y = Percent, fill = Corpus)) +
  geom_col(position = "dodge") +
  labs(title = "Word Order Frequency by Corpus", y = "Percentage") +
  theme_minimal()

# 2. PIE CHART: Modern English word order proportions
ModernEnglish %>%
  mutate(label = paste0(Order, ": ", round(Percent, 1), "%")) %>%
  ggplot(aes(x = "", y = Percent, fill = label)) +  # use label in legend
  geom_bar(stat = "identity", width = 1) +
  coord_polar("y") +
  labs(title = "Modern English Word Order Proportions", fill = "Word Order") +
  theme_void() +
  theme(legend.position = "right")

# 2. PIE CHART: Old English word order proportions
OldEnglish %>%
  mutate(label = paste0(Order, ": ", round(Percent, 1), "%")) %>%
  ggplot(aes(x = "", y = Percent, fill = label)) +  # use label in legend
  geom_bar(stat = "identity", width = 1) +
  coord_polar("y") +
  labs(title = "Old English Word Order Proportions", fill = "Word Order") +
  theme_void() +
  theme(legend.position = "right")

# 3. SIDE-BY-SIDE COMPARISON (Percent)
ggplot(all_data, aes(x = Order, y = Percent, fill = Corpus)) +
  geom_col(position = "dodge") +
  labs(title = "Relative Word Order Usage", y = "Percentage") +
  theme_minimal()

# 4. RADAR CHART (OPTIONAL, more visual)
# Prepare radar input
radar_df <- all_data %>%
  select(Order, Percent, Corpus) %>%
  pivot_wider(names_from = Order, values_from = Percent, values_fill = 0)

radar_data <- as.data.frame(rbind(rep(100, 6), rep(0, 6), radar_df[,-1]))
rownames(radar_data) <- c("Max", "Min", radar_df$Corpus)

radarchart(radar_data,
           axistype = 1,
           pcol = c("red", "blue"),
           plwd = 2,
           plty = 1,
           cglcol = "grey", cglty = 1,
           axislabcol = "black", caxislabels = seq(0, 100, 20), cglwd = 0.8,
           vlcex = 0.8)
legend("topright", legend = radar_df$Corpus, col = c("red", "blue"), lty = 1, lwd = 2)

# 5. CUMULATIVE % PLOT (sorted by freq)
all_data %>%
  group_by(Corpus) %>%
  arrange(desc(Percent)) %>%
  mutate(Cumulative = cumsum(Percent)) %>%
  ggplot(aes(x = reorder(Order, -Percent), y = Cumulative, group = Corpus, color = Corpus)) +
  geom_line(size = 1.2) +
  geom_point() +
  labs(title = "Cumulative Word Order Coverage", y = "Cumulative %") +
  theme_minimal()

