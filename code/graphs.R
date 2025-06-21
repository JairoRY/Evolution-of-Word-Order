# 0. Packages 
library(ggplot2)
library(readr)
library(dplyr)
library(tidyr)
library(scales) 
#install.packages("fmsb")
library(fmsb)

# 1. Read data ---------------------------------------------------
ModernEnglish <- read_csv("word_order_stats_Susanne.csv") %>%
  mutate(Corpus = "ModernEnglish")

OldEnglish    <- read_csv("word_order_stats_PCEEC.csv") %>%
  mutate(Corpus = "OldEnglish")

all_data <- bind_rows(ModernEnglish, OldEnglish)

# 2. BAR CHART (percentage, side‑by‑side) -----------------------
bar_plot <- ggplot(all_data,
                   aes(x = Order, y = Percent, fill = Corpus)) +
  geom_col(position = "dodge") +
  scale_y_continuous(labels = percent_format(scale = 1)) +
  labs(title = "Word‑order frequency by corpus",
       y = "Percentage of clauses") +
  theme_minimal()

ggsave("bar_word_order.pdf", bar_plot,
       device = cairo_pdf, width = 5, height = 4)

# 3. PIE CHARTS (legend only) ------------------------------------
pie_mod <- ModernEnglish %>%
  mutate(lbl = paste0(Order, ": ", round(Percent, 1), "%")) %>%
  ggplot(aes(x = "", y = Percent, fill = lbl)) +
  geom_col(width = 1, color = "white") +
  coord_polar("y") +
  labs(title = "Modern English word‑order proportions",
       fill = NULL) +
  theme_void() +
  theme(legend.position = "right")

ggsave("pie_modern_english.pdf", pie_mod,
       device = cairo_pdf, width = 4, height = 4)

pie_old <- OldEnglish %>%
  mutate(lbl = paste0(Order, ": ", round(Percent, 1), "%")) %>%
  ggplot(aes(x = "", y = Percent, fill = lbl)) +
  geom_col(width = 1, color = "white") +
  coord_polar("y") +
  labs(title = "Old English word‑order proportions",
       fill = NULL) +
  theme_void() +
  theme(legend.position = "right")

ggsave("pie_old_english.pdf", pie_old,
       device = cairo_pdf, width = 4, height = 4)
