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

# 4. CUMULATIVE COVERAGE -----------------------------------------
cum_plot <- all_data %>%
  group_by(Corpus) %>%
  arrange(desc(Percent)) %>%
  mutate(Cumulative = cumsum(Percent)) %>%
  ggplot(aes(x = reorder(Order, -Percent),
             y = Cumulative,
             group = Corpus,
             color = Corpus)) +
  geom_line(size = 1.2) +
  geom_point() +
  scale_y_continuous(labels = percent_format(scale = 1),
                     limits = c(0, 100)) +
  labs(title = "Cumulative coverage of word‑order patterns",
       y = "Cumulative % of clauses", x = "Order") +
  theme_minimal()

ggsave("cumulative_word_order.pdf", cum_plot,
       device = cairo_pdf, width = 5, height = 4)

# 5. RADAR (optional, visual) ------------------------------------
#    Radar needs both rows (max/min) + corpora rows
radar_df <- all_data %>%
  select(Order, Percent, Corpus) %>%
  pivot_wider(names_from = Order, values_from = Percent,
              values_fill = 0)

radar_data <- rbind(
  Max = rep(100, ncol(radar_df) - 1),
  Min = rep(0,   ncol(radar_df) - 1),
  radar_df[,-1]
)
rownames(radar_data)[3:4] <- radar_df$Corpus

# customise colours
par(family = "sans")
cairo_pdf("radar_word_order.pdf", width = 5, height = 5)   # vector PDF
radarchart(radar_data,
           axistype = 1,
           pcol = c("#E41A1C", "#377EB8"), plwd = 2, plty = 1,
           cglcol = "grey70", cglty = 1, cglwd = 0.8,
           axislabcol = "grey50", caxislabels = seq(0, 100, 20),
           vlcex = 0.8)
legend("topright", legend = radar_df$Corpus,
       col = c("#E41A1C", "#377EB8"), lty = 1, lwd = 2, bty = "n")
dev.off()


